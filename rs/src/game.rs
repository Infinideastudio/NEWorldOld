//! Minimum-viable game state — the bare wiring that makes the Rust port
//! actually playable.
//!
//! This is the placeholder for the full `[F] GameApp` orchestrator described
//! in `docs/rust_migration.md` §4.16. It deliberately skips `[E]` (UI, menus,
//! HUD beyond the debug line) and `[F]` (raycast, breaking, async chunk
//! pipeline, screenshots, save/load wiring) so the demo can stand up against
//! a static, fully-generated world rendered every frame.
//!
//! Owns:
//!
//! * a [`World`] generated synchronously at startup, sized by
//!   [`RENDER_DISTANCE`];
//! * one [`ChunkMesh`] per loaded chunk, built once via [`mesh_chunk`] and
//!   uploaded as a wgpu vertex buffer;
//! * a free-fly [`Camera`] driven by WSAD + mouse-look;
//! * the [`ChunkPipeline`] / [`ParticlePipeline`] / [`DepthTarget`] from `[D]`;
//! * an empty [`ParticleSystem`] (no spawners yet — the system ticks but is
//!   inert until `[F]` adds block-break particles).

use std::path::PathBuf;
use std::sync::Arc;

use cgmath::{InnerSpace, Matrix4, Point3, Rad, Vector3};

use crate::blocks::{BaseBlocks, BlockData, BlockRegistry, register_base_blocks};
use crate::chunks::Chunk;
use crate::gfx::{
    Atlases, ChunkMesh, ChunkPipeline, DepthTarget, FrameUniforms, MeshInput, PADDED_SIZE,
    PADDED_VOLUME, ParticleMesh, ParticlePipeline, UniformBuffer, mat4_to_array, mesh_chunk,
    padded_index,
};
use crate::input::{InputState, Key};
use crate::math::{Vec3d, Vec3i};
use crate::particles::ParticleSystem;
use crate::worlds::{BlockView, World, WorldError};

/// Render radius in chunks (axis-aligned cube). Total = `(2N+1)^3` chunks.
/// 3 → 343 chunks → ~1.4 M cells. Picks the largest size that meshes in
/// well under a second on a single core.
pub const RENDER_DISTANCE: i32 = 3;

/// Sky / surface clear color. Matches `chunk.wgsl`'s `SKY_COLOR` so distant
/// fog blends seamlessly into the cleared background.
pub const SKY_COLOR: wgpu::Color = wgpu::Color {
    r: 0.55,
    g: 0.72,
    b: 0.92,
    a: 1.0,
};

/// World seed for deterministic terrain. Fixed for the demo.
const WORLD_SEED: u32 = 0x00C0_FFEE;

/// World coord the chunk grid centers on at startup. The terrain surface in
/// `Generator::height` lives around `y ≈ 120` (a bit above the
/// `WATER_LEVEL = 96` baseline), so anchoring the chunk window here gives
/// the player something interesting to look at. With `RENDER_DISTANCE = 3`
/// the loaded chunks span world `y ∈ [80, 191]`.
const WORLD_CENTER: cgmath::Vector3<i32> = cgmath::Vector3::new(0, 128, 0);

/// Initial camera position — above the terrain surface, slightly back so the
/// player isn't spawned inside a block.
const INITIAL_CAMERA: Vec3d = cgmath::Vector3::new(0.0, 160.0, 32.0);

/// Default sun direction (normalized at write-time).
fn default_sun_dir() -> Vector3<f32> {
    Vector3::new(0.4, 0.8, 0.5).normalize()
}

/// `glm`-style Y-up free-fly camera.
///
/// Yaw rotates around the world `+Y`; positive yaw turns left (CCW from
/// above). Pitch is around the camera's local X axis; positive looks up.
/// Both are radians.
#[derive(Debug, Clone)]
pub struct Camera {
    pub position: Vec3d,
    pub yaw: f64,
    pub pitch: f64,
    pub fov_y: f32,
    pub near: f32,
    pub far: f32,
    /// Movement speed in blocks/second.
    pub speed: f64,
    /// Mouse sensitivity in radians per pixel of motion.
    pub mouse_sensitivity: f64,
}

impl Camera {
    #[must_use]
    pub fn new(position: Vec3d) -> Self {
        Self {
            position,
            yaw: 0.0,
            pitch: -0.35,
            fov_y: 70.0_f32.to_radians(),
            near: 0.1,
            far: 1024.0,
            speed: 18.0,
            mouse_sensitivity: 0.0025,
        }
    }

    /// Unit forward vector in world space.
    #[must_use]
    pub fn forward(&self) -> Vec3d {
        let cy = self.yaw.cos();
        let sy = self.yaw.sin();
        let cp = self.pitch.cos();
        let sp = self.pitch.sin();
        Vec3d::new(-sy * cp, sp, -cy * cp)
    }

    /// Unit right vector (perpendicular to forward in the horizontal plane).
    #[must_use]
    pub fn right(&self) -> Vec3d {
        let cy = self.yaw.cos();
        let sy = self.yaw.sin();
        Vec3d::new(cy, 0.0, -sy)
    }

    /// Right-handed view matrix.
    #[must_use]
    pub fn view_matrix(&self) -> Matrix4<f32> {
        let eye = Point3::new(
            self.position.x as f32,
            self.position.y as f32,
            self.position.z as f32,
        );
        let f = self.forward();
        let dir = Vector3::new(f.x as f32, f.y as f32, f.z as f32);
        Matrix4::look_to_rh(eye, dir, Vector3::unit_y())
    }

    /// Perspective projection matrix in wgpu's clip-space convention
    /// (Z in `[0, 1]`).
    #[must_use]
    pub fn proj_matrix(&self, aspect: f32) -> Matrix4<f32> {
        OPENGL_TO_WGPU * cgmath::perspective(Rad(self.fov_y), aspect, self.near, self.far)
    }

    /// Apply WSAD-space-shift movement and pitch/yaw mouse-look.
    pub fn update_from_input(&mut self, input: &InputState, dt: f32) {
        let dx = f64::from(input.mouse_motion.x);
        let dy = f64::from(input.mouse_motion.y);
        self.yaw -= dx * self.mouse_sensitivity;
        self.pitch -= dy * self.mouse_sensitivity;
        let limit = std::f64::consts::FRAC_PI_2 - 0.01;
        self.pitch = self.pitch.clamp(-limit, limit);

        let forward = self.forward();
        let right = self.right();
        let up = Vec3d::new(0.0, 1.0, 0.0);

        let mut dir = Vec3d::new(0.0, 0.0, 0.0);
        if input.is_key_down(Key::W) {
            dir += forward;
        }
        if input.is_key_down(Key::S) {
            dir -= forward;
        }
        if input.is_key_down(Key::D) {
            dir += right;
        }
        if input.is_key_down(Key::A) {
            dir -= right;
        }
        if input.is_key_down(Key::Space) {
            dir += up;
        }
        if input.is_key_down(Key::LeftShift) {
            dir -= up;
        }

        let mag2 = dir.magnitude2();
        if mag2 > 1e-6 {
            dir /= mag2.sqrt();
        }
        let speed = if input.is_key_down(Key::LeftControl) {
            self.speed * 5.0
        } else {
            self.speed
        };
        self.position += dir * speed * f64::from(dt);
    }
}

/// `Matrix4` that maps GL clip space `Z in [-1, 1]` to wgpu's `[0, 1]`.
/// Pre-multiply against any `cgmath::perspective` result.
#[rustfmt::skip]
const OPENGL_TO_WGPU: Matrix4<f32> = Matrix4::new(
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 0.5, 0.0,
    0.0, 0.0, 0.5, 1.0,
);

/// All the game-side state, owned by `App`.
pub struct Game {
    pub world: World,
    pub camera: Camera,
    pub chunk_meshes: Vec<ChunkMesh>,
    pub chunk_pipeline: ChunkPipeline,
    pub depth: DepthTarget,
    pub particles: ParticleSystem,
    pub particle_mesh: ParticleMesh,
    pub particle_pipeline: ParticlePipeline,
    pub sun_dir: Vector3<f32>,
}

impl Game {
    /// Build the world (synchronously, all chunks within `RENDER_DISTANCE`),
    /// mesh every chunk, upload as a `ChunkMesh`, and stand up the [D]
    /// pipelines.
    #[allow(clippy::too_many_arguments)] // app-level wiring is naturally wide
    pub fn new(
        device: &wgpu::Device,
        _queue: &wgpu::Queue,
        surface_format: wgpu::TextureFormat,
        surface_size: (u32, u32),
        registry: &Arc<BlockRegistry>,
        base_blocks: BaseBlocks,
        frame_uniforms: &UniformBuffer<FrameUniforms>,
        atlases: &Atlases,
    ) -> Result<Self, WorldError> {
        let world_dir = ensure_world_root();
        let world_name = format!("mvp-{}", std::process::id());
        tracing::info!(?world_dir, world_name, "creating world");

        let mut world = World::new(
            world_name,
            RENDER_DISTANCE,
            WORLD_SEED,
            Arc::clone(registry),
            base_blocks,
        )?;
        world.set_center(WORLD_CENTER);

        // Pump synchronous chunk loading until no more loads happen.
        let target = ((2 * RENDER_DISTANCE + 1) as usize).pow(3);
        let mut prev = 0;
        let mut spins = 0;
        while world.chunks().len() < target && spins < 256 {
            world.tick_chunk_loading();
            let now = world.chunks().len();
            if now == prev {
                spins += 1;
            } else {
                spins = 0;
                prev = now;
            }
        }
        tracing::info!(loaded = world.chunks().len(), target, "world loaded");

        // Mesh every chunk in our radius.
        let chunk_meshes = build_all_chunk_meshes(device, &world, registry, &base_blocks);
        tracing::info!(count = chunk_meshes.len(), "chunk meshes uploaded");

        let depth = DepthTarget::new(device, surface_size.0.max(1), surface_size.1.max(1));

        let chunk_pipeline = ChunkPipeline::new(
            device,
            surface_format,
            DepthTarget::FORMAT,
            frame_uniforms,
            atlases,
        );
        let particle_pipeline = ParticlePipeline::new(
            device,
            surface_format,
            DepthTarget::FORMAT,
            frame_uniforms,
            atlases,
        );

        let particles = ParticleSystem::new();
        let mut particle_mesh = ParticleMesh::new();
        particle_mesh.rebuild(device, particles.particles());

        let camera = Camera::new(INITIAL_CAMERA);

        Ok(Self {
            world,
            camera,
            chunk_meshes,
            chunk_pipeline,
            depth,
            particles,
            particle_mesh,
            particle_pipeline,
            sun_dir: default_sun_dir(),
        })
    }

    /// Per-frame simulation. `dt` is real elapsed seconds since the last tick.
    pub fn tick(&mut self, dt: f32, input: &InputState) {
        self.camera.update_from_input(input, dt);
        // Field-disjoint borrow so particles can read the world via BlockView
        // while we mutate particles.
        let Self {
            world, particles, ..
        } = self;
        let view: &dyn BlockView = world;
        particles.tick(dt, &BlockViewRef(view));
    }

    /// React to a window resize: resize the depth attachment.
    pub fn resize(&mut self, device: &wgpu::Device, width: u32, height: u32) {
        self.depth.resize(device, width.max(1), height.max(1));
    }

    /// Update `FrameUniforms` with the latest camera + sun + screen-size
    /// snapshot. Call once per frame, before [`Self::record_world_pass`].
    pub fn write_frame_uniforms(
        &self,
        queue: &wgpu::Queue,
        frame_uniforms: &UniformBuffer<FrameUniforms>,
        surface_size: (u32, u32),
        elapsed: f32,
    ) {
        let (w, h) = surface_size;
        let aspect = w.max(1) as f32 / h.max(1) as f32;
        let view = self.camera.view_matrix();
        let proj = self.camera.proj_matrix(aspect);
        let view_proj = proj * view;

        let mut u = FrameUniforms::default();
        u.view = mat4_to_array(view);
        u.proj = mat4_to_array(proj);
        u.view_proj = mat4_to_array(view_proj);
        u.camera_pos = [
            self.camera.position.x as f32,
            self.camera.position.y as f32,
            self.camera.position.z as f32,
            1.0,
        ];
        u.sun_dir = [self.sun_dir.x, self.sun_dir.y, self.sun_dir.z, 0.0];
        u.screen_size = [w as f32, h as f32];
        u.time = elapsed;
        frame_uniforms.write(queue, &u);
    }

    /// Record the world (chunks + particles) render pass into `encoder`,
    /// drawing into `color_view` with the owned [`DepthTarget`].
    pub fn record_world_pass(
        &mut self,
        device: &wgpu::Device,
        encoder: &mut wgpu::CommandEncoder,
        color_view: &wgpu::TextureView,
    ) {
        // Particle vertex buffer — rebuild per-frame from the current
        // particle list (cheap when empty; a one-buffer write otherwise).
        self.particle_mesh
            .rebuild(device, self.particles.particles());

        let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("game.world_pass"),
            color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                view: color_view,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations {
                    load: wgpu::LoadOp::Clear(SKY_COLOR),
                    store: wgpu::StoreOp::Store,
                },
            })],
            depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                view: &self.depth.view,
                depth_ops: Some(wgpu::Operations {
                    load: wgpu::LoadOp::Clear(1.0),
                    store: wgpu::StoreOp::Store,
                }),
                stencil_ops: None,
            }),
            timestamp_writes: None,
            occlusion_query_set: None,
            multiview_mask: None,
        });

        self.chunk_pipeline.begin_opaque(&mut pass);
        for cm in &self.chunk_meshes {
            cm.draw_opaque(&mut pass);
        }
        self.chunk_pipeline.begin_translucent(&mut pass);
        for cm in &self.chunk_meshes {
            cm.draw_translucent(&mut pass);
        }
        self.particle_pipeline.render(&mut pass, &self.particle_mesh);
    }
}

/// Newtype that lets us pass a `&dyn BlockView` through `&impl BlockView`
/// constraints (needed by `ParticleSystem::tick`'s generic parameter).
struct BlockViewRef<'a>(&'a dyn BlockView);

impl BlockView for BlockViewRef<'_> {
    fn block(&self, coord: Vec3i) -> Option<BlockData> {
        self.0.block(coord)
    }
    fn block_or_air(&self, coord: Vec3i) -> BlockData {
        self.0.block_or_air(coord)
    }
    fn hitboxes(&self, box_: crate::math::Aabb3d) -> Vec<crate::math::Aabb3d> {
        self.0.hitboxes(box_)
    }
    fn in_water(&self, box_: crate::math::Aabb3d) -> bool {
        self.0.in_water(box_)
    }
}

/// Set CWD to a temp directory under `std::env::temp_dir()` so the world's
/// sled DB doesn't litter wherever the binary was launched from.
fn ensure_world_root() -> PathBuf {
    let dir = std::env::temp_dir().join("neworld-mvp");
    let _ = std::fs::create_dir_all(&dir);
    if std::env::set_current_dir(&dir).is_err() {
        tracing::warn!(?dir, "failed to chdir into temp world root; using cwd");
    }
    dir
}

/// Build a [`MeshInput`] for `ccoord` by sampling the world's blocks at every
/// padded cell. Out-of-range neighbors return `block_or_air`'s air default.
fn build_mesh_input(world: &World, ccoord: Vec3i) -> MeshInput {
    let air = BlockData::default();
    // Heap-allocate without putting the array on the stack first.
    let v = vec![air; PADDED_VOLUME];
    let boxed: Box<[BlockData]> = v.into_boxed_slice();
    let mut padded: Box<[BlockData; PADDED_VOLUME]> = match boxed.try_into() {
        Ok(arr) => arr,
        Err(_) => unreachable!("PADDED_VOLUME constant is correct"),
    };

    let chunk_origin = Vec3i::new(
        ccoord.x * Chunk::SIZE,
        ccoord.y * Chunk::SIZE,
        ccoord.z * Chunk::SIZE,
    );
    let p_size = i32::try_from(PADDED_SIZE).unwrap_or(18);
    for pz in 0..p_size {
        for py in 0..p_size {
            for px in 0..p_size {
                let global = chunk_origin + Vec3i::new(px - 1, py - 1, pz - 1);
                let block = world.block_or_air(global);
                let idx = padded_index(px as usize, py as usize, pz as usize);
                padded[idx] = block;
            }
        }
    }

    MeshInput {
        coord: ccoord,
        padded,
    }
}

/// Mesh and upload every chunk currently in the slab.
fn build_all_chunk_meshes(
    device: &wgpu::Device,
    world: &World,
    registry: &BlockRegistry,
    _base: &BaseBlocks,
) -> Vec<ChunkMesh> {
    let mut out = Vec::with_capacity(world.chunks().len());
    let coords: Vec<Vec3i> = world.chunks().iter().map(|(_, c)| c.coord()).collect();
    for ccoord in coords {
        let input = build_mesh_input(world, ccoord);
        let mesh = mesh_chunk(&input, registry);
        if mesh.opaque.is_empty() && mesh.translucent.is_empty() {
            continue;
        }
        out.push(ChunkMesh::upload(device, &mesh));
    }
    out
}

/// Build a fresh registry + base blocks. Wraps the registry in `Arc` so
/// background workers can hold cheap clones.
#[must_use]
pub fn build_block_registry() -> (Arc<BlockRegistry>, BaseBlocks) {
    let mut registry = BlockRegistry::new();
    let base = register_base_blocks(&mut registry);
    (Arc::new(registry), base)
}

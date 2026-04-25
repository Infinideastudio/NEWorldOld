//! Game state — the orchestrator that owns the world, the camera, and the
//! GPU mesh cache, and drives the per-frame simulation.
//!
//! Implements `[F2]`–`[F6]` from `docs/rust_migration.md` §5:
//!
//! * `[F2]` — block selection raycast, breaking, and placing.
//! * `[F3]` — chat dispatch via [`crate::commands::CommandRegistry`].
//! * `[F5]` — async chunk loading: [`World::tick_chunk_loading_async`] +
//!   [`World::poll_load_results`] are pumped per frame; chunks stream into
//!   the slab over the first few frames.
//! * `[F6]` — async chunk meshing: dirty coords ship to a worker via
//!   [`MeshPipeline`] and the resulting `MeshOutput`s are uploaded to GPU as
//!   they arrive (see [`Self::pump_meshing`]).
//!
//! Owns:
//!
//! * a [`World`] whose chunk window is sized by [`RENDER_DISTANCE`];
//! * a coord-keyed map of [`ChunkMesh`]es so async-mesh delivery can find
//!   the right slot in O(1);
//! * a free-fly [`Camera`] driven by WSAD + mouse-look;
//! * the [`ChunkPipeline`] / [`ParticlePipeline`] / [`DepthTarget`] from `[D]`;
//! * a [`ParticleSystem`] (block-break particles spawn into it);
//! * a [`CommandRegistry`] populated by [`register_base_commands`];
//! * the current selection [`Hit`] and the current view-projection matrix
//!   (used by the HUD to draw a selection outline overlay).

pub mod camera;
pub mod raycast;

use std::collections::{HashMap, HashSet};
use std::path::PathBuf;
use std::sync::Arc;
use std::time::Instant;

use cgmath::{InnerSpace, Matrix4, SquareMatrix, Vector3};

use crate::blocks::{BaseBlocks, BlockData, BlockRegistry, register_base_blocks};
use crate::chunks::Chunk;
use crate::commands::{CommandRegistry, register_base_commands};
use crate::gfx::{
    Atlases, ChunkMesh, ChunkPipeline, DepthTarget, FrameUniforms, MeshInput, MeshPipeline,
    PADDED_SIZE, PADDED_VOLUME, ParticleMesh, ParticlePipeline, UniformBuffer, mat4_to_array,
    padded_index,
};
use crate::input::{InputState, MouseButton};
use crate::math::{Vec3d, Vec3i};
use crate::particles::{Particle, ParticleSystem};
use crate::worlds::{BlockView, World, WorldError};

pub use camera::Camera;
pub use raycast::{Hit, RAYCAST_MAX};

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

/// Initial camera position — above the terrain surface (which lives around
/// `y ≈ 120` in `Generator::height`), slightly back so the player isn't
/// spawned inside a block. `Game::new` seeds the world's load center from
/// this; thereafter `tick_sim` keeps the center pinned to the player's
/// chunk so loads/unloads follow the camera.
const INITIAL_CAMERA: Vec3d = cgmath::Vector3::new(0.0, 160.0, 32.0);

/// Half-extent of the camera's collision AABB, in blocks. Mirrors the C++
/// player hitbox radius used to reject placements that would trap the player.
const CAMERA_HALF_EXTENT: f64 = 0.3;

/// Lifetime of a freshly-spawned chat message before it fades from the
/// auto-decay panel. `chat_open` overrides this — open chat always shows
/// recent history.
const CHAT_MESSAGE_LIFETIME_SECS: f32 = 5.0;

/// Number of debris particles spawned per block break.
const PARTICLES_PER_BREAK: u32 = 10;

/// Default sun direction (normalized at write-time).
fn default_sun_dir() -> Vector3<f32> {
    Vector3::new(0.4, 0.8, 0.5).normalize()
}

/// Maximum new mesh jobs to issue per frame. Caps the per-frame CPU spike
/// when many chunks land at once (e.g. on the first frame, or after a fast
/// teleport that invalidates everything in the load window).
const MAX_MESH_DISPATCHES_PER_FRAME: usize = 8;

/// All the game-side state, owned by `App`.
pub struct Game {
    pub world: World,
    pub camera: Camera,
    /// Per-coord GPU mesh. `HashMap` keyed by chunk coord so dirty-chunk
    /// rebuilds (`[F2]`) and async-mesh delivery (`[F6]`) can swap one slot in
    /// O(1), and chunk unload can drop the entry by coord without touching a
    /// stale `ChunkKey`.
    pub chunk_meshes: HashMap<Vec3i, ChunkMesh>,
    pub chunk_pipeline: ChunkPipeline,
    pub depth: DepthTarget,
    pub particles: ParticleSystem,
    pub particle_mesh: ParticleMesh,
    pub particle_pipeline: ParticlePipeline,
    pub sun_dir: Vector3<f32>,
    /// Currently-selected block (raycast hit). Updated each tick.
    pub selected: Option<Hit>,
    /// Latest computed view-projection matrix; the HUD reads it to draw the
    /// selection outline. Mirrored from the value uploaded into
    /// `FrameUniforms` the same frame.
    pub view_proj: Matrix4<f32>,
    /// Off-thread mesher ([F6]). Drained per frame by [`Self::pump_meshing`].
    mesh_worker: MeshPipeline,
    /// Set of chunk coords that need their mesh rebuilt. Walked per frame and
    /// shipped to `mesh_worker` (throttled by [`MAX_MESH_DISPATCHES_PER_FRAME`]).
    dirty_chunks: HashSet<Vec3i>,
    /// Coords currently in flight on the mesh worker.
    meshing_in_flight: HashSet<Vec3i>,
    /// Chat lines + the time they were posted.
    pub chat_messages: Vec<(String, Instant)>,
    /// `[F3]` — slash-command dispatch table.
    pub commands: CommandRegistry,
    /// Block registry shared with the mesher.
    registry: Arc<BlockRegistry>,
    /// `BaseBlocks` ids needed by the raycast predicate and break/place logic.
    pub base_blocks: BaseBlocks,
    /// Tiny LCG state used to jitter break-particle positions / velocities.
    /// We deliberately avoid pulling in the `rand` crate — the spec calls for
    /// "a tiny LCG; do NOT add a `rand` dep".
    rng: u64,
}

impl Game {
    /// Build the world (chunks stream in asynchronously over the next few
    /// frames via [F5]/[F6]) and stand up the [D] pipelines.
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
        render_distance: i32,
    ) -> Result<Self, WorldError> {
        let world_root = world_root();
        let world_name = format!("mvp-{}", std::process::id());
        tracing::info!(?world_root, world_name, render_distance, "creating world");

        let mut world = World::new_at(
            &world_root,
            world_name,
            render_distance,
            WORLD_SEED,
            Arc::clone(registry),
            base_blocks,
        )?;
        // Pin the load center to the spawn chunk so the first set of load
        // requests covers the player's actual surroundings. `tick_sim` keeps
        // the center following the camera from then on.
        world.set_center(camera_world_coord(INITIAL_CAMERA));
        // Kick the async load pipeline ([F5]) so chunks start streaming in on
        // frame 1. Meshes follow once load results arrive (see `tick`).
        world.tick_chunk_loading_async();

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
        let mesh_worker = MeshPipeline::spawn(Arc::clone(registry));

        let mut commands = CommandRegistry::new();
        register_base_commands(&mut commands, &base_blocks, Arc::clone(registry));

        Ok(Self {
            world,
            camera,
            chunk_meshes: HashMap::new(),
            chunk_pipeline,
            depth,
            particles,
            particle_mesh,
            particle_pipeline,
            sun_dir: default_sun_dir(),
            selected: None,
            view_proj: Matrix4::identity(),
            mesh_worker,
            dirty_chunks: HashSet::new(),
            meshing_in_flight: HashSet::new(),
            chat_messages: Vec::new(),
            commands,
            registry: Arc::clone(registry),
            base_blocks,
            rng: 0x9E37_79B9_7F4A_7C15,
        })
    }

    /// Render-rate per-frame update. `dt` is real elapsed seconds since the
    /// last render frame, **not** the simulation step length. Runs every
    /// frame so mouse-look + WSAD + selection raycast respond immediately
    /// (rather than at the 30 Hz simulation rate, which makes both feel
    /// laggy at high render FPS).
    ///
    /// `chat_open` and `paused` come from the UI layer; while either is true
    /// we suppress the break/place mouse handling and camera input so menu /
    /// chat clicks don't double up as world edits.
    pub fn tick_render(
        &mut self,
        dt: f32,
        input: &InputState,
        chat_open: bool,
        paused: bool,
    ) {
        if !chat_open && !paused {
            self.camera.update_from_input(input, dt);
        }

        // Selection raycast is cheap and feels nicer when it tracks the
        // camera at full frame rate.
        self.update_selection();

        // Mouse → break/place. Press edges only fire on the frame they
        // happened, so we have to consume them at frame rate too — running
        // this in `tick_sim` would drop clicks that happened mid-frame.
        if !chat_open && !paused {
            if input.is_mouse_button_pressed(MouseButton::Left) {
                self.try_break();
            }
            if input.is_mouse_button_pressed(MouseButton::Right) {
                self.try_place();
            }
        }
    }

    /// Fixed-step simulation. `dt` is the simulation step length (1/30 s);
    /// callers run this in an accumulator loop in [`crate::app::App::frame`].
    /// Particle physics, chunk pipeline polling, and the world's load center
    /// follow live here — anything where rate-stability matters more than
    /// input latency.
    pub fn tick_sim(&mut self, dt: f32) {
        // Particle physics — gravity / drag / lifetime. Field-disjoint
        // borrow so the simulation can read world blocks via BlockView.
        {
            let Self {
                world, particles, ..
            } = self;
            let view: &dyn BlockView = world;
            particles.tick(dt, &BlockViewRef(view));
        }

        // Slide the chunk grid + height map to follow the player. Mirrors
        // C++ `update_chunk_lists`: only re-pivot when the player crosses a
        // chunk boundary so we don't churn the height-map cache every tick.
        let player_world = camera_world_coord(self.camera.position);
        let player_chunk = crate::worlds::chunk_coord(player_world);
        if self.world.center_ccoord() != player_chunk {
            self.world.set_center(player_world);
        }

        // Drive the async chunk pipeline (F5): issue load/unload requests.
        self.world.tick_chunk_loading_async();
        let inserted = self.world.poll_load_results();
        if !inserted.is_empty() {
            mark_dirty_with_neighbours(&mut self.dirty_chunks, &inserted);
        }

        // Drop cached meshes for any coord that's no longer loaded — happens
        // every tick (not just on insert) so unload-on-walk reaps GPU
        // buffers as the player moves out of range. Borrow-split: collect
        // stale coords first, then mutate.
        let stale: Vec<Vec3i> = self
            .chunk_meshes
            .keys()
            .copied()
            .filter(|c| self.world.chunk_by_coord(*c).is_none())
            .collect();
        for c in stale {
            self.chunk_meshes.remove(&c);
            // Also clear the dirty queue so we don't dispatch a mesh job
            // for a chunk that was just unloaded.
            self.dirty_chunks.remove(&c);
        }
    }

    /// Drive the async meshing pipeline (F6): submit up to N dirty coords to
    /// the mesh worker, then drain finished meshes back into `chunk_meshes`.
    /// Call once per frame from `App::frame`, after [`Self::tick`]. Splits
    /// from `tick` because the upload step needs `&wgpu::Device`.
    pub fn pump_meshing(&mut self, device: &wgpu::Device) {
        // ---- dispatch dirty meshes ----
        let mut to_dispatch: Vec<Vec3i> = Vec::new();
        for &coord in &self.dirty_chunks {
            if to_dispatch.len() >= MAX_MESH_DISPATCHES_PER_FRAME {
                break;
            }
            if self.meshing_in_flight.contains(&coord) {
                continue;
            }
            if self.world.chunk_by_coord(coord).is_none() {
                continue; // not loaded yet — leave the dirty entry in place
            }
            to_dispatch.push(coord);
        }
        for coord in &to_dispatch {
            let input = build_mesh_input(&self.world, *coord);
            if self.mesh_worker.submit(input) {
                self.meshing_in_flight.insert(*coord);
                self.dirty_chunks.remove(coord);
            }
        }

        // ---- drain finished meshes ----
        for done in self.mesh_worker.drain() {
            let coord = done.output.coord;
            self.meshing_in_flight.remove(&coord);
            // Re-resolve by coord (§2.5). A stale ChunkKey is never used.
            if self.world.chunk_by_coord(coord).is_none() {
                self.chunk_meshes.remove(&coord);
                continue;
            }
            if done.output.opaque.is_empty() && done.output.translucent.is_empty() {
                self.chunk_meshes.remove(&coord);
                continue;
            }
            let mesh = ChunkMesh::upload(device, &done.output);
            self.chunk_meshes.insert(coord, mesh);
        }
    }

    /// React to a window resize: resize the depth attachment.
    pub fn resize(&mut self, device: &wgpu::Device, width: u32, height: u32) {
        self.depth.resize(device, width.max(1), height.max(1));
    }

    /// Update `FrameUniforms` with the latest camera + sun + screen-size
    /// snapshot. Call once per frame, before [`Self::record_world_pass`].
    pub fn write_frame_uniforms(
        &mut self,
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
        self.view_proj = view_proj;

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
        for cm in self.chunk_meshes.values() {
            cm.draw_opaque(&mut pass);
        }
        self.chunk_pipeline.begin_translucent(&mut pass);
        for cm in self.chunk_meshes.values() {
            cm.draw_translucent(&mut pass);
        }
        self.particle_pipeline.render(&mut pass, &self.particle_mesh);
    }

    /// Run a chat / slash command line. If it begins with `/`, dispatches
    /// through the [`CommandRegistry`]; otherwise echoes the raw text into
    /// chat history. World mutations made by the command are picked up via
    /// `Chunk::modified()` and added to the dirty-chunk set.
    pub fn submit_chat_line(&mut self, line: String) {
        if line.is_empty() {
            return;
        }
        if line.starts_with('/') {
            // Take a "before" snapshot of `modified()` so we can detect every
            // chunk the command touched. The registry expects a flat
            // `&mut Vec<String>` for output — we drain that into our
            // timestamped chat buffer afterwards.
            let before: HashMap<Vec3i, bool> = self
                .world
                .chunks()
                .iter()
                .map(|(_, c)| (c.coord(), c.modified()))
                .collect();
            let mut out = Vec::<String>::new();
            self.commands.execute_on(&line, &mut self.world, &mut out);
            for s in out {
                self.push_chat(s);
            }
            // Mark every chunk whose modified flag flipped or that's new.
            let after: HashMap<Vec3i, bool> = self
                .world
                .chunks()
                .iter()
                .map(|(_, c)| (c.coord(), c.modified()))
                .collect();
            let mut dirtied: Vec<Vec3i> = Vec::new();
            for (coord, now_mod) in &after {
                let was_mod = before.get(coord).copied().unwrap_or(false);
                if *now_mod && !was_mod {
                    dirtied.push(*coord);
                }
            }
            for coord in dirtied {
                self.mark_chunk_dirty_with_neighbors(coord);
            }
        } else {
            self.push_chat(line);
        }
    }

    /// Push a chat line stamped with `Instant::now()`. Used both by
    /// `submit_chat_line` and by the registry callbacks (commands push chat
    /// output via the `messages` argument).
    pub fn push_chat(&mut self, line: String) {
        self.chat_messages.push((line, Instant::now()));
        // Bound history to a reasonable cap so we don't grow forever.
        if self.chat_messages.len() > 64 {
            let extra = self.chat_messages.len() - 64;
            self.chat_messages.drain(0..extra);
        }
    }

    /// Recompute the selected block from the camera. The "non-empty"
    /// predicate is `id != air && id != water` — water is treated as empty
    /// for break/place purposes.
    fn update_selection(&mut self) {
        let origin = self.camera.position;
        let dir = self.camera.forward();
        let air = self.base_blocks.air;
        let water = self.base_blocks.water;
        self.selected = raycast::raycast(&self.world, origin, dir, RAYCAST_MAX, |w, c| {
            let id = w.block_or_air(c).id;
            id != air && id != water
        });
    }

    /// Break the currently-selected block.
    fn try_break(&mut self) {
        let Some(hit) = self.selected else {
            return;
        };
        let coord = hit.coord;
        let block = self.world.block_or_air(coord);
        if block.id == self.base_blocks.air {
            return;
        }
        // Record texture layer before mutation so the particles inherit the
        // broken block's face art.
        let tex_layer = u32::from(self.registry.get(block.id).face(0).0);

        self.world.set_block(coord, self.base_blocks.air, true);
        self.mark_chunk_dirty_with_neighbors(crate::worlds::chunk_coord(coord));

        // Spawn debris.
        for _ in 0..PARTICLES_PER_BREAK {
            let px = f64::from(coord.x) + self.rand_unit();
            let py = f64::from(coord.y) + self.rand_unit();
            let pz = f64::from(coord.z) + self.rand_unit();
            let vx = (self.rand_unit() - 0.5) * 0.4;
            let vy = self.rand_unit() * 0.3;
            let vz = (self.rand_unit() - 0.5) * 0.4;
            self.particles.spawn(Particle::new(
                Vec3d::new(px, py, pz),
                Vec3d::new(vx, vy, vz),
                tex_layer,
                1.0,
            ));
        }
    }

    /// Place a block onto the face the ray entered through.
    fn try_place(&mut self) {
        let Some(hit) = self.selected else {
            return;
        };
        let target = hit.coord + hit.normal;
        // Reject placement that would overlap the camera's AABB.
        if camera_overlaps_block(self.camera.position, target) {
            return;
        }
        // Don't replace existing solid blocks (lets `+normal` placement work
        // even if the normal happens to be zero — the ray-start-inside-solid
        // case).
        let existing = self.world.block_or_air(target).id;
        if existing != self.base_blocks.air && existing != self.base_blocks.water {
            return;
        }
        // For the MVP, the held block is always stone.
        self.world.set_block(target, self.base_blocks.stone, true);
        self.mark_chunk_dirty_with_neighbors(crate::worlds::chunk_coord(target));
    }

    /// Mark the chunk at `cc` and any neighbour chunk that touches the
    /// modified cell as dirty.
    fn mark_chunk_dirty_with_neighbors(&mut self, cc: Vec3i) {
        self.dirty_chunks.insert(cc);
        // Always queue all 6 neighbour chunks (cheap; the per-frame budget
        // bounds the actual remesh cost). Block-edge cases (faces touching
        // the chunk boundary) need the neighbour's mesh refreshed too.
        for off in NEIGHBOR_OFFSETS {
            self.dirty_chunks.insert(cc + off);
        }
    }

    /// Tiny linear-congruential PRNG returning a value in `[0, 1)`. Avoids
    /// pulling in the `rand` crate — the break-particle jitter doesn't need
    /// statistical quality.
    fn rand_unit(&mut self) -> f64 {
        // Numerical Recipes constants.
        self.rng = self.rng.wrapping_mul(6_364_136_223_846_793_005).wrapping_add(1_442_695_040_888_963_407);
        // Take the high 53 bits and divide by 2^53.
        ((self.rng >> 11) as f64) / 9_007_199_254_740_992.0
    }
}

/// Check whether the camera's AABB (centered on `pos` with half-extent
/// [`CAMERA_HALF_EXTENT`]) intersects the unit block AABB at `block`.
/// Convert a camera world-space position into the integer block coord it's
/// standing in. Mirrors the C++ player-coord rounding.
fn camera_world_coord(pos: Vec3d) -> Vec3i {
    Vec3i::new(
        pos.x.floor() as i32,
        pos.y.floor() as i32,
        pos.z.floor() as i32,
    )
}

fn camera_overlaps_block(pos: Vec3d, block: Vec3i) -> bool {
    let lo = Vec3d::new(f64::from(block.x), f64::from(block.y), f64::from(block.z));
    let hi = Vec3d::new(
        f64::from(block.x + 1),
        f64::from(block.y + 1),
        f64::from(block.z + 1),
    );
    let cam_lo = pos - Vec3d::new(CAMERA_HALF_EXTENT, CAMERA_HALF_EXTENT, CAMERA_HALF_EXTENT);
    let cam_hi = pos + Vec3d::new(CAMERA_HALF_EXTENT, CAMERA_HALF_EXTENT, CAMERA_HALF_EXTENT);
    cam_lo.x < hi.x
        && cam_hi.x > lo.x
        && cam_lo.y < hi.y
        && cam_hi.y > lo.y
        && cam_lo.z < hi.z
        && cam_hi.z > lo.z
}

/// 6-neighbor chunk offsets, used to mark neighbouring chunks dirty.
const NEIGHBOR_OFFSETS: [Vec3i; 6] = [
    Vec3i::new(1, 0, 0),
    Vec3i::new(-1, 0, 0),
    Vec3i::new(0, 1, 0),
    Vec3i::new(0, -1, 0),
    Vec3i::new(0, 0, 1),
    Vec3i::new(0, 0, -1),
];

/// Mark each coord in `inserted` and its 6 axis-aligned neighbours dirty.
/// Faces along the shared chunk boundary may need to update once a new chunk
/// arrives, so the neighbours' meshes are also re-issued. Used by the async
/// load result handler ([F5]) — break/place go through
/// [`Game::mark_chunk_dirty_with_neighbors`] instead.
fn mark_dirty_with_neighbours(dirty: &mut HashSet<Vec3i>, inserted: &[Vec3i]) {
    for &c in inserted {
        dirty.insert(c);
        for off in NEIGHBOR_OFFSETS {
            dirty.insert(c + off);
        }
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

/// Root directory for the demo world's files (`<temp>/neworld-mvp/`).
/// Returned as an absolute path; `World::new_at` and `save_to_disk` use it
/// directly so the binary leaves no artefacts in the launch directory and
/// nothing depends on cwd.
fn world_root() -> PathBuf {
    let dir = std::env::temp_dir().join("neworld-mvp");
    let _ = std::fs::create_dir_all(&dir);
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

/// Build a fresh registry + base blocks. Wraps the registry in `Arc` so
/// background workers can hold cheap clones.
#[must_use]
pub fn build_block_registry() -> (Arc<BlockRegistry>, BaseBlocks) {
    let mut registry = BlockRegistry::new();
    let base = register_base_blocks(&mut registry);
    (Arc::new(registry), base)
}

impl Game {
    /// Filter chat history to lines whose timestamp is within the recent
    /// decay window, plus everything when chat is open. Public for the HUD.
    #[must_use]
    pub fn visible_chat_lines(&self, chat_open: bool) -> Vec<&str> {
        let now = Instant::now();
        let cutoff = std::time::Duration::from_secs_f32(CHAT_MESSAGE_LIFETIME_SECS);
        // Show at most the last 8 lines.
        self.chat_messages
            .iter()
            .rev()
            .take(8)
            .filter(|(_, t)| chat_open || now.saturating_duration_since(*t) < cutoff)
            .map(|(s, _)| s.as_str())
            .collect::<Vec<_>>()
            .into_iter()
            .rev()
            .collect()
    }
}

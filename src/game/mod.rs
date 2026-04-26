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
//! Player / camera split:
//! * The [`crate::worlds::Player`] owns the simulation state — position,
//!   velocity, orientation, gravity, hitbox collision, jump bookkeeping. Its
//!   `update(world)` runs in [`Self::tick_sim`] at 30 Hz.
//! * The [`Camera`] is a passive view: every frame, [`Self::tick_render`]
//!   mirrors `player.orientation()` into `camera.yaw/pitch`, and
//!   [`Self::write_frame_uniforms`] reads the interpolated eye position
//!   (`player.look_coord() - velocity * (1 - interp)`) into `camera.position`.
//!
//! Owns:
//!
//! * a [`World`] whose chunk window is sized by the configured render distance;
//! * a coord-keyed map of [`ChunkMesh`]es so async-mesh delivery can find
//!   the right slot in O(1);
//! * a [`Camera`] whose state is pushed in from the player every frame;
//! * the [`ChunkPipeline`] / [`ParticlePipeline`] / [`DepthTarget`] from `[D]`;
//! * a [`ParticleSystem`] (block-break particles spawn into it);
//! * a [`CommandRegistry`] populated by [`register_base_commands`];
//! * the current selection [`Hit`] and the current view-projection matrix
//!   (used by the HUD to draw a selection outline overlay).

pub mod camera;
pub mod raycast;

use std::collections::{HashMap, HashSet};
use std::path::Path;
use std::sync::Arc;
use std::time::Instant;

use cgmath::{InnerSpace, Matrix4, SquareMatrix, Vector3};

use crate::blocks::{BaseBlocks, BlockData, BlockRegistry, register_base_blocks};
use crate::chunks::Chunk;
use crate::commands::{CommandRegistry, register_base_commands};
use crate::input::{InputState, Key, MouseButton};
use crate::items::ItemStack;
use crate::math::{Vec3d, Vec3i};
use crate::particles::{Particle, ParticleSystem};
use crate::render::{
    DepthTarget, FrameUniforms, MeshInput, MeshPipeline, PADDED_SIZE, PADDED_VOLUME, ParticleMesh,
    ParticlePipeline, UniformBuffer, mat4_to_array, padded_index,
};
use crate::textures::Atlases;
use crate::worlds::chunk_rendering::{ChunkMesh, ChunkPipeline};
use crate::worlds::{BlockView, GameMode, World, WorldError};

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

/// Half-extent of the player-collision rejection box used by `try_place`.
/// Matches the C++ `Player::aabb` x/z half-width — placements that would
/// drop a solid block into the player's hitbox are silently rejected.
const PLAYER_HALF_EXTENT_HORIZ: f64 = 0.3;

/// Lifetime of a freshly-spawned chat message before it fades from the
/// auto-decay panel. `chat_open` overrides this — open chat always shows
/// recent history.
const CHAT_MESSAGE_LIFETIME_SECS: f32 = 5.0;

/// Number of debris particles spawned per block break.
const PARTICLES_PER_BREAK: u32 = 10;

/// Maximum delay between two W-presses that still counts as a sprint
/// double-tap. Matches the C++ `0.5` second window in `neworld.ixx::game_update`.
const SPRINT_DOUBLE_TAP_SECS: f64 = 0.5;

/// Per-pixel mouse-look gain in radians. The TOML `mouse_speed` value (≈0.1)
/// times this constant gives the effective rad/pixel mapping. Matches the
/// C++ formula `MouseSpeed * π / 180 * Δpx` evaluated for a 1° step.
const MOUSE_LOOK_RAD_PER_PIXEL: f64 = std::f64::consts::PI / 180.0;

/// Default sun direction (normalized at write-time).
fn default_sun_dir() -> Vector3<f32> {
    Vector3::new(0.4, 0.8, 0.5).normalize()
}

/// Maximum new mesh jobs to issue per frame. Caps the per-frame CPU spike
/// when many chunks land at once (e.g. on the first frame, or after a fast
/// teleport that invalidates everything in the load window).
const MAX_MESH_DISPATCHES_PER_FRAME: usize = 8;

/// `sqrt(3)` to f32 precision. Used by `write_frame_uniforms` to scale the
/// fog distance to the diagonal of the loaded chunk cube. `f32::sqrt` is
/// `const` in newer toolchains but not in our stable target, so we keep
/// this as a literal to avoid a runtime call per frame.
const SQRT_3_F32: f32 = 1.732_050_8;

/// All the game-side state, owned by `App`.
pub struct Game {
    pub world: World,
    pub camera: Camera,
    /// Per-coord GPU mesh. Keyed by chunk coord so dirty-chunk rebuilds
    /// (`[F2]`) and async-mesh delivery (`[F6]`) can swap one entry in O(1),
    /// and chunk unload can drop the entry by coord directly.
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
    /// Tracks the last time the W key was pressed (`Instant`-monotonic, seconds
    /// since `Game::new`). Mirrors `wPressTimer` in `neworld.ixx::game_update`
    /// so a W-press within `SPRINT_DOUBLE_TAP_SECS` of the previous one
    /// triggers sprinting.
    last_w_press: Option<Instant>,
    /// Mouse sensitivity multiplier sourced from `Config.mouse_speed`. Pushed
    /// in by the app's `apply_config` each frame.
    pub mouse_speed: f64,
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
        worlds_root: &Path,
        world_name: String,
        world_seed: u32,
    ) -> Result<Self, WorldError> {
        tracing::info!(?worlds_root, world_name, world_seed, render_distance, "creating world");

        let mut world = World::new_at(
            worlds_root,
            world_name,
            render_distance,
            world_seed,
            Arc::clone(registry),
            base_blocks,
        )?;

        // Try to restore the player from disk. If the file is missing or
        // unreadable, the default player at spawn is left in place.
        let player_path = world.player_path();
        if player_path.exists() {
            match crate::worlds::Player::load_from(&player_path) {
                Ok(p) => {
                    *world.player_mut() = p;
                    tracing::info!(?player_path, "player restored");
                }
                Err(err) => tracing::warn!(error = %err, ?player_path, "player load failed, using defaults"),
            }
        }

        // Pin the load center to the player chunk so the first set of load
        // requests covers the player's actual surroundings. `tick_sim` keeps
        // the center following the player from then on.
        let player_world = world.player().coord();
        world.set_center(Vec3i::new(
            player_world.x.floor() as i32,
            player_world.y.floor() as i32,
            player_world.z.floor() as i32,
        ));
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

        // Camera starts at the eye position; tick_render keeps it synced.
        let look = world.player().look_coord();
        let mut camera = Camera::new(look);
        camera.set_orientation(world.player().orientation());

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
            last_w_press: None,
            mouse_speed: 0.1,
        })
    }

    /// Render-rate per-frame update. `dt` is real elapsed seconds since the
    /// last render frame, **not** the simulation step length. Runs every
    /// frame so mouse-look + selection raycast respond immediately rather
    /// than at the 30 Hz simulation rate (which makes both feel laggy at
    /// high render FPS).
    ///
    /// `chat_open` and `inventory_open` and `paused` come from the UI layer;
    /// while any is true we suppress mouse-look + break/place + hotbar
    /// scrolling so menu / chat clicks don't double up as world edits.
    pub fn tick_render(
        &mut self,
        _dt: f32,
        input: &InputState,
        chat_open: bool,
        inventory_open: bool,
        paused: bool,
    ) {
        let ui_modal = chat_open || inventory_open || paused;
        if !ui_modal {
            // Mouse-look — apply directly to the player's orientation. The
            // C++ uses `MouseSpeed * π / 180 * Δpx`; we mirror that scaling
            // exactly so the TOML `mouse_speed` value carries the same
            // meaning across builds.
            let dx = f64::from(input.mouse_motion.x);
            let dy = f64::from(input.mouse_motion.y);
            if dx != 0.0 || dy != 0.0 {
                let gain = self.mouse_speed * MOUSE_LOOK_RAD_PER_PIXEL;
                let mut o = self.world.player().orientation();
                o.heading -= dx * gain;
                o.pitch -= dy * gain;
                self.world.player_mut().set_orientation(o);
            }
        }

        // Camera mirrors the player's orientation each frame, regardless of
        // pause / chat — UI overlays still want a coherent view of the world
        // behind them. Camera position is set in `write_frame_uniforms` so it
        // can include the simulation interpolation factor.
        self.camera.set_orientation(self.world.player().orientation());
        self.camera.position = self.world.player().look_coord();

        // Selection raycast is cheap and feels nicer when it tracks the
        // camera at full frame rate.
        self.update_selection();

        // Mouse → break/place + hotbar scroll. Press edges only fire on the
        // frame they happened, so we have to consume them at frame rate too —
        // running this in `tick_sim` would drop clicks that happened mid-frame.
        if !ui_modal {
            if input.is_mouse_button_pressed(MouseButton::Left) {
                self.try_break();
            }
            if input.is_mouse_button_pressed(MouseButton::Right) {
                self.try_place();
            }
            // Hotbar cycling. Z/X step through the 10 slots; mouse wheel
            // scrolls (positive = up = previous slot, mirroring most voxel
            // games' convention).
            if input.is_key_pressed(Key::Z) {
                let cur = self.world.player().held_item_stack_index();
                self.world
                    .player_mut()
                    .set_held_item_stack_index((cur + 9) % 10);
            }
            if input.is_key_pressed(Key::X) {
                let cur = self.world.player().held_item_stack_index();
                self.world
                    .player_mut()
                    .set_held_item_stack_index((cur + 1) % 10);
            }
            let wheel = input.mouse_wheel_delta;
            if wheel.abs() >= 0.5 {
                let cur = self.world.player().held_item_stack_index();
                let steps = wheel.round() as i32;
                let next = ((cur as i32 - steps).rem_euclid(10)) as usize;
                self.world.player_mut().set_held_item_stack_index(next);
            }
            // Mode toggles. F1 → game mode, F4 → cross-wall (creative only).
            if input.is_key_pressed(Key::F1) {
                let next = match self.world.player().game_mode() {
                    GameMode::Survival => GameMode::Creative,
                    GameMode::Creative => GameMode::Survival,
                };
                self.world.player_mut().set_game_mode(next);
            }
            if input.is_key_pressed(Key::F4) {
                let cw = self.world.player().cross_wall();
                self.world.player_mut().set_cross_wall(!cw);
            }
        }
    }

    /// Fixed-step simulation. `dt` is the simulation step length (1/30 s);
    /// callers run this in an accumulator loop in [`crate::app::App::frame`].
    /// Particle physics, chunk pipeline polling, the world's load center, and
    /// the player's physics tick all run here — anything where rate stability
    /// matters more than input latency.
    ///
    /// `first_tick_of_frame` is `true` for the first slice the accumulator
    /// drains in a given frame; subsequent ticks see `false`. Press-edge keys
    /// (Space, Enter…) are only processed on the first tick so a slow frame
    /// that drains multiple ticks doesn't fire a single keypress repeatedly.
    pub fn tick_sim(
        &mut self,
        dt: f32,
        input: &InputState,
        first_tick_of_frame: bool,
        chat_open: bool,
        inventory_open: bool,
        paused: bool,
    ) {
        let ui_modal = chat_open || inventory_open || paused;
        // Particle physics — gravity / drag / lifetime. Field-disjoint
        // borrow so the simulation can read world blocks via BlockView.
        {
            let Self {
                world, particles, ..
            } = self;
            let view: &dyn BlockView = world;
            particles.tick(dt, &BlockViewRef(view));
        }

        // Player input + physics. Mirrors the C++ `game_update` flow:
        //   1. WSAD adds horizontal velocity in the player heading frame.
        //   2. Space / Shift drive jump / crouch.
        //   3. `player.update(world)` damps, applies gravity, and clips
        //      against block hitboxes.
        if !ui_modal {
            self.process_movement_input(input, first_tick_of_frame);
        } else {
            // Stop sprint if input is gated this tick; matches C++
            // `set_running(false)` when W is not held.
            self.world.player_mut().set_running(false);
            self.last_w_press = None;
        }
        // Physics runs unconditionally — gravity should keep applying while
        // a menu is open, just like the C++ build. `World::update_player`
        // hides the player/world borrow split internally (see its docs).
        self.world.update_player();

        // Slide the chunk grid + height map to follow the player. Mirrors
        // C++ `update_chunk_lists`: only re-pivot when the player crosses a
        // chunk boundary so we don't churn the height-map cache every tick.
        let player_world = self.world.player().coord();
        let player_block = Vec3i::new(
            player_world.x.floor() as i32,
            player_world.y.floor() as i32,
            player_world.z.floor() as i32,
        );
        let player_chunk = crate::worlds::chunk_coord(player_block);
        if self.world.center_ccoord() != player_chunk {
            self.world.set_center(player_block);
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
            .filter(|c| !self.world.is_loaded(*c))
            .collect();
        for c in stale {
            self.chunk_meshes.remove(&c);
            // Also clear the dirty queue so we don't dispatch a mesh job
            // for a chunk that was just unloaded.
            self.dirty_chunks.remove(&c);
        }
    }

    /// Process WSAD / Space / Shift / sprint detection and feed the player.
    /// Press-edge events only fire when `first_tick_of_frame` is true.
    fn process_movement_input(&mut self, input: &InputState, first_tick_of_frame: bool) {
        let player = self.world.player();
        let heading = player.orientation().heading;
        let speed = player.speed();
        let flying = player.flying();
        let cross_wall = player.cross_wall();

        // Direction unit vectors derived purely from heading (the C++
        // walking model: WSAD is horizontal regardless of pitch).
        let (sin_h, cos_h) = heading.sin_cos();
        let forward = Vec3d::new(-sin_h, 0.0, -cos_h);
        let right = Vec3d::new(cos_h, 0.0, -sin_h);

        let mut delta_v = Vec3d::new(0.0, 0.0, 0.0);
        if input.is_key_down(Key::W) {
            delta_v += forward * speed;
        }
        if input.is_key_down(Key::S) {
            delta_v -= forward * speed;
        }
        if input.is_key_down(Key::D) {
            delta_v += right * speed;
        }
        if input.is_key_down(Key::A) {
            delta_v -= right * speed;
        }

        if delta_v.x != 0.0 || delta_v.z != 0.0 {
            let mut velocity = self.world.player().velocity();
            velocity += delta_v;
            // Walking speed cap — only when grounded / non-flying. Matches
            // C++: horizontal velocity magnitude clipped to `speed`.
            if !flying && !cross_wall {
                let xz_mag2 = velocity.x * velocity.x + velocity.z * velocity.z;
                if xz_mag2 > speed * speed {
                    let inv = speed / xz_mag2.sqrt();
                    velocity.x *= inv;
                    velocity.z *= inv;
                }
            }
            self.world.player_mut().set_velocity(velocity);
        }

        // Sprint detection — double-tap W within `SPRINT_DOUBLE_TAP_SECS`.
        if first_tick_of_frame {
            let w_pressed = input.is_key_pressed(Key::W);
            if w_pressed {
                let now = Instant::now();
                let sprinted = match self.last_w_press {
                    Some(prev) if now.duration_since(prev).as_secs_f64() <= SPRINT_DOUBLE_TAP_SECS => {
                        self.world.player_mut().set_running(true);
                        true
                    }
                    _ => false,
                };
                self.last_w_press = if sprinted { None } else { Some(now) };
            }
        }
        // Releasing W stops the sprint (mirrors C++ `set_running(false)`).
        if !input.is_key_down(Key::W) {
            self.world.player_mut().set_running(false);
            self.last_w_press = None;
        }

        // Jump — Space. on_jump consumes a mid-air jump only on press edge.
        if input.is_key_down(Key::Space) {
            let just_pressed = first_tick_of_frame && input.is_key_pressed(Key::Space);
            self.world.player_mut().on_jump(just_pressed);
        }
        // Crouch — Shift held.
        if input.is_key_down(Key::LeftShift) || input.is_key_down(Key::RightShift) {
            self.world.player_mut().on_crouch();
        }
    }

    /// Drive the async meshing pipeline (F6): submit up to N dirty coords to
    /// the mesh worker, then drain finished meshes back into `chunk_meshes`.
    /// Call once per frame from `App::frame`, after [`Self::tick_sim`]. Splits
    /// from the simulation tick because the upload step needs `&wgpu::Device`.
    pub fn pump_meshing(&mut self, device: &wgpu::Device) {
        // ---- dispatch dirty meshes, closest first ----
        // `dirty_chunks` is a `HashSet`, so iterating it directly produces
        // hash-order which can leave nearby chunks unmeshed while distant
        // ones get the budget — visible as fragmented pop-in at high
        // render distance. Sort by squared chunk-distance to the player so
        // the visible neighbourhood meshes first.
        let player_world = self.world.player().coord();
        let player_chunk = crate::worlds::chunk_coord(Vec3i::new(
            player_world.x.floor() as i32,
            player_world.y.floor() as i32,
            player_world.z.floor() as i32,
        ));
        let mut candidates: Vec<(i32, Vec3i)> = self
            .dirty_chunks
            .iter()
            .filter(|c| !self.meshing_in_flight.contains(c))
            .filter(|c| self.world.is_loaded(**c))
            .map(|&c| {
                let d = c - player_chunk;
                (d.x * d.x + d.y * d.y + d.z * d.z, c)
            })
            .collect();
        candidates.sort_by_key(|(d, _)| *d);
        candidates.truncate(MAX_MESH_DISPATCHES_PER_FRAME);

        for (_, coord) in candidates {
            let input = build_mesh_input(&self.world, coord);
            if self.mesh_worker.submit(input) {
                self.meshing_in_flight.insert(coord);
                self.dirty_chunks.remove(&coord);
            }
        }

        // ---- drain finished meshes ----
        for done in self.mesh_worker.drain() {
            let coord = done.output.coord;
            self.meshing_in_flight.remove(&coord);
            // Re-resolve by coord — the chunk may have been unloaded while
            // its mesh was in flight on the worker.
            if !self.world.is_loaded(coord) {
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
    /// `tick_alpha` is the `[0, 1)` accumulator fraction since the last
    /// simulation tick — used to interpolate the eye position so render-rate
    /// motion is smooth even though physics ticks at 30 Hz. C++ flow: see
    /// `neworld.ixx::render_scene` (`view_coord = look - velocity * (1-α)`).
    pub fn write_frame_uniforms(
        &mut self,
        queue: &wgpu::Queue,
        frame_uniforms: &UniformBuffer<FrameUniforms>,
        surface_size: (u32, u32),
        elapsed: f32,
        tick_alpha: f32,
    ) {
        // Interpolated eye position. Mirrors the C++ formula from
        // `render_scene`: at α=0 (tick just happened) we show the pre-tick
        // position so motion is continuous; at α=1 (about to tick again) we
        // show the post-tick position.
        let player = self.world.player();
        let look = player.look_coord();
        let velocity = player.velocity();
        let alpha = f64::from(tick_alpha.clamp(0.0, 1.0));
        let eye = look - velocity * (1.0 - alpha);
        self.camera.position = eye;
        self.camera.set_orientation(player.orientation());

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
        // Fog scales with the live render distance: `fog_end` lands just past
        // the diagonal of the loaded cube (so corner chunks fade smoothly into
        // sky), `fog_start` 65% of the way there for a generous fade band.
        let r = self.world.render_distance() as f32;
        let chunk = Chunk::SIZE as f32;
        u.fog_end = (r + 0.5) * SQRT_3_F32 * chunk;
        u.fog_start = u.fog_end * 0.65;
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
            // Only non-empty chunks can be modified — `Chunk::block_mut` is
            // the only path that flips `modified`, and it allocates the
            // data array (so the chunk is non-empty by then).
            let before: HashMap<Vec3i, bool> = self
                .world
                .non_empty_chunks()
                .map(|(coord, c)| (coord, c.modified()))
                .collect();
            let mut out = Vec::<String>::new();
            self.commands.execute_on(&line, &mut self.world, &mut out);
            for s in out {
                self.push_chat(s);
            }
            // Mark every chunk whose modified flag flipped or that's new.
            let after: HashMap<Vec3i, bool> = self
                .world
                .non_empty_chunks()
                .map(|(coord, c)| (coord, c.modified()))
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

    /// Break the currently-selected block. The broken block is added to the
    /// player's inventory as a single-item stack (mirrors the C++
    /// `player.add_item({selb, 1})` in `neworld.ixx::game_update`).
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

        // Drop the broken block into the player's inventory. `add_item`
        // returns false if the inventory is completely full of incompatible
        // stacks; in that case the item is lost (matching the C++ behaviour
        // — there is no in-world dropped-item entity to fall back to yet).
        let _ = self
            .world
            .player_mut()
            .add_item(ItemStack::new(block.id, 1));

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

    /// Place the currently-held hotbar block onto the face the ray entered
    /// through. Decrements the held stack by one. No-op when the held slot
    /// is empty.
    fn try_place(&mut self) {
        let Some(hit) = self.selected else {
            return;
        };
        let target = hit.coord + hit.normal;
        // Hotbar slot's id determines what gets placed. Empty hotbar = no
        // placement (mirrors C++: `if (!holdingItem.empty())`).
        let hotbar = *self.world.player().held_item_stack();
        if hotbar.empty() {
            return;
        }
        let placed_id = hotbar.id;
        // Don't try to place air or non-solid placeholder ids.
        if placed_id == self.base_blocks.air {
            return;
        }
        // Reject placement that would overlap the player's hitbox. C++ does
        // this via `Player::put_block`, which calls `aabb().intersects(...)`;
        // we replicate the same shape here.
        if player_overlaps_block(&self.world, target) {
            return;
        }
        // Don't replace existing solid blocks (lets `+normal` placement work
        // even if the normal happens to be zero — the ray-start-inside-solid
        // case).
        let existing = self.world.block_or_air(target).id;
        if existing != self.base_blocks.air && existing != self.base_blocks.water {
            return;
        }
        self.world.set_block(target, placed_id, true);
        self.mark_chunk_dirty_with_neighbors(crate::worlds::chunk_coord(target));

        // Decrement the hotbar stack. In creative we could keep the stack
        // full, but the C++ build always decrements — so we mirror that.
        let idx = self.world.player().held_item_stack_index();
        let slot = self.world.player_mut().inventory_item_stack_mut(3, idx);
        if slot.count > 0 {
            slot.count -= 1;
            if slot.count == 0 {
                *slot = ItemStack::default();
            }
        }
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

/// Predicate: would placing a unit-cube block at `block` overlap the player's
/// hitbox right now? Mirrors the player-collision check in C++
/// `Player::put_block` (`player_aabb.intersects(block_aabb)`).
fn player_overlaps_block(world: &World, block: Vec3i) -> bool {
    let player = world.player();
    if player.cross_wall() {
        return false;
    }
    let coord = player.coord();
    let lo = Vec3d::new(f64::from(block.x), f64::from(block.y), f64::from(block.z));
    let hi = Vec3d::new(
        f64::from(block.x + 1),
        f64::from(block.y + 1),
        f64::from(block.z + 1),
    );
    let p_lo = Vec3d::new(coord.x - PLAYER_HALF_EXTENT_HORIZ, coord.y, coord.z - PLAYER_HALF_EXTENT_HORIZ);
    let p_hi = Vec3d::new(coord.x + PLAYER_HALF_EXTENT_HORIZ, coord.y + 1.7, coord.z + PLAYER_HALF_EXTENT_HORIZ);
    p_lo.x < hi.x
        && p_hi.x > lo.x
        && p_lo.y < hi.y
        && p_hi.y > lo.y
        && p_lo.z < hi.z
        && p_hi.z > lo.z
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


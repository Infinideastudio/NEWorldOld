# Rust Port — Design and Feature Parity

This document describes the Rust port of NEWorld and how it relates to the
C++23 original (preserved under [`old/`](../old/) for diffing). It covers:

1. The shape of the C++ codebase as it stands.
2. The shape of the Rust codebase as it stands.
3. A module-by-module feature-parity report.
4. A roadmap to close the remaining gaps.

The migration is well past the bring-up phase — all seven of the original
implementation groups (`[A]`–`[F]`) have shipped and the binary is end-to-end
playable. What remains is renderer polish and a few small wiring tasks.

---

## 1. C++ design (under `old/`)

The C++ original is a single executable built from C++23 modules. ~12 K LoC
across `old/src/*.{ixx,cpp}` plus ~1.1 K lines of GLSL across
`old/shaders/`. CMake + vcpkg drive the build; the only third-party-pinned
deps are GLFW, GLAD, FreeType, GLM, LevelDB, libpng/zlib, and a handful of
header-only vcpkg ports.

### Layout

| Module | Role |
|---|---|
| `math/{vector,matrix,aabb,euler,frustum}.ixx` | Templated linear algebra + AABB + frustum + Euler. |
| `blocks.ixx` | `Id`, `State`, `Light`, `BlockData`, `BlockInfo`, `BlockInfoRegistry`, `BaseBlocks`. |
| `items.ixx` | `ItemStack`. |
| `chunks.ixx` | `Chunk` with lazy `unique_ptr<array<…>>` data, terrain entry point, package/unpackage. |
| `chunk_pointer_arrays.ixx` | 3D sliding-window `Chunk*` cache to bypass the chunk hashmap on hot paths. |
| `terrain_generation.ixx` + `height_maps.ixx` | Fractal-noise generator + 2D height cache (each carries module-level mutable state). |
| `worlds/worlds.ixx` | `World`: `_chunks` hashmap + parallel `_renders` map, `TilesStore` (LevelDB), block-update queue, chunk load/unload/meshing pipelines. |
| `worlds/player.{ixx,cpp}` | `Player` + physics + raw-`reinterpret_cast` save/load. |
| `worlds/{chunk,world}_rendering.cpp` | Greedy meshing routine and per-chunk draw dispatch. |
| `commands.ixx` | 12 base slash-commands. |
| `globalization.ixx` + `lang/*.lang` | i18n table (singleton). |
| `particles.ixx` | Particle list + sim + mesh (module-mutable state). |
| `text_rendering.ixx` | FreeType atlas + uploader (module-mutable state). |
| `textures.ixx` | Texture globals + per-block face → atlas index table. |
| `ui/{context,element,layout,render,controls/*}.ixx` | Bespoke Flutter-style declarative UI library (View/Element/Builder, Row/Column/Stack, Sizer/Padding, Button/Slider/TextBox/ImageBox/ScrollView). |
| `menus/{main,world,create_world,game,options,render_options,shader_options,ui_options,language}_menu.cpp` | Modal menus built on the `ui` library. |
| `render/{buffer,texture,framebuffer,program,vertex_array,attrib_*,block_*,image}.ixx` | OpenGL RAII wrappers + compile-time vertex/uniform-block layout descriptors. |
| `rendering.ixx` | `Renderer` namespace: shader/texture/framebuffer arrays, per-pass dispatch, sun heading, reversed-Z, sRGB framebuffer setup. |
| `globals.ixx` | ~50 free `export` mutable variables (window, mouse/key state, render distance, game time, RNG seed, …). |
| `setup.ixx` | GLFW init, callbacks (which write straight into `globals.ixx`), fullscreen, splash. |
| `neworld.ixx` (53 KB, single largest file) | The "god file": `main()`, update thread, input dispatch, block-pick raycast, HUD, inventory, screenshot/thumbnail, breaking overlay, chat. |

### Key properties of the C++ side

- **Globals everywhere.** Window / input / time / RNG / GPU pipeline are all
  module-level exports.
- **Two-thread update/render.** `std::jthread` runs the simulation tick at 30 Hz
  alongside the render thread; a `std::mutex` guards world state.
- **Two parallel chunk maps + a sliding-window cache.** `_chunks: unordered_map<ChunkId,
  unique_ptr<Chunk>>` (canonical owner) + `_renders` (GPU mesh state, keyed
  identically) + `ChunkPointerArray` (3D sliding window of raw `Chunk*` to
  bypass the hash on hot paths).
- **Deferred renderer.** Opaque/translucent passes write a G-buffer; `final.fsh`
  (641 lines, the largest single shader) composes the result with fog, sky,
  volumetric clouds, soft shadows, SSR, and post-blur.
- **LevelDB chunk store**, binary `reinterpret_cast`-based player save.
- **Bespoke UI library** (`ui/*.ixx`, ~1.9 K LoC) that the menus build on.

---

## 2. Rust design (under repo root)

Single Cargo crate, edition 2024, Rust 1.95+. ~14 K LoC of Rust + 242 lines of
WGSL. All graphics / windowing / audio deps are pure-Rust crates; no external
system libraries. License: CC0-1.0.

### Layout

```
src/
├── lib.rs / main.rs / setup.rs    crate root, entry point, tracing init
├── app.rs                         winit ApplicationHandler, fixed-step
│                                  accumulator, WorldAction queue, save on exit
│
├── blocks.rs                      ↔ blocks.ixx
├── config.rs                      ↔ globals.ixx (options part)
├── globalization.rs               ↔ globalization.ixx
├── height_maps.rs                 ↔ height_maps.ixx
├── input.rs                       InputState (no winit dep)
├── items.rs                       ↔ items.ixx
├── particles.rs                   ↔ particles.ixx
├── terrain_generation.rs          ↔ terrain_generation.ixx
├── text_rendering.rs              ↔ text_rendering.ixx (glyphon-backed)
├── textures.rs                    ↔ textures.ixx
│
├── chunks/{mod,generate}.rs       ↔ chunks.ixx
├── commands/{mod,base}.rs         ↔ commands.ixx (12 base commands)
├── math/{mod,aabb,euler,frustum}.rs   ↔ math/*.ixx
│
├── game/                          in-process game orchestrator
│   ├── mod.rs                     tick_render / tick_sim / pump_meshing /
│   │                              break / place / chat dispatch
│   ├── camera.rs                  passive view; mirrors player
│   └── raycast.rs                 Amanatides–Woo voxel DDA
│
├── menus/                         ↔ menus/*.cpp
│   ├── main_menu.rs / world_menu.rs / create_world_menu.rs
│   ├── options_menu.rs            consolidated; see §3
│   └── game_menu.rs               composes HUD + inventory + pause
│
├── render/                        wgpu replacement for old/src/render/
│   ├── context.rs                 Gfx (instance/adapter/device/queue/surface)
│   ├── basic_pipeline.rs          bring-up scaffold
│   ├── depth.rs                   DepthTarget
│   ├── egui_renderer.rs           egui ↔ wgpu bridge
│   ├── mesh.rs                    CPU per-face culled meshing
│   ├── mesh_pipeline.rs           off-thread mesh worker
│   ├── particle_render.rs         billboard particle pipeline
│   ├── screenshot.rs              surface readback → PNG
│   └── uniforms.rs                FrameUniforms / ModelUniforms / FilterUniforms
│
├── ui/                            Rust-specific UI infra
│   ├── action.rs                  WorldActionQueue (cross-screen lifecycle)
│   ├── hud.rs                     crosshair / debug / chat / selection
│   ├── inventory.rs               4×10 grid + always-on hotbar + mouse pickup
│   └── screen.rs                  Screen trait + ScreenStack
│
└── worlds/
    ├── chunk_rendering.rs         ↔ worlds/chunk_rendering.cpp (ChunkMesh
    │                              + ChunkPipeline)
    ├── player/{mod,save}.rs       ↔ player.ixx + player_impl.cpp
    └── world/                     ↔ worlds.ixx
        ├── mod.rs
        ├── error.rs               WorldError (thiserror)
        ├── pipeline.rs            async chunk load/save worker
        └── store.rs               TilesStore (sled)

shaders/
├── basic.wgsl                     bring-up
├── chunk.wgsl                     opaque + translucent (shared); fog +
│                                  ambient sky baked in
└── particle.wgsl                  billboards
```

### Key properties of the Rust side

- **No globals.** `App` is created in `main`, threaded through winit's
  `ApplicationHandler`. `Config` is `Arc<Mutex<…>>`, `BlockRegistry` is
  `Arc<…>` (read-only after init). Window state lives on `App`; per-frame
  input lives in `InputState`.
- **Single-threaded fixed-step loop.** `App::frame` runs a 30 Hz accumulator
  (`TICK_DT = 1/30`, `MAX_TICKS_PER_FRAME = 5`); `Game::tick_sim` runs once
  per slice, `Game::tick_render` runs every frame so mouse-look stays smooth
  at high FPS.
- **Coord-keyed chunk storage.** `World::chunks: HashMap<Vec3i, Chunk>` is
  the sole owner; the C++ `_chunks` + `_renders` + `ChunkPointerArray`
  trio collapses into this single map. GPU mesh state lives in
  `Game::chunk_meshes: HashMap<Vec3i, ChunkMesh>` — separate from `World`
  so the world layer stays graphics-free.
- **`non_empty` side-set + invariant.** A parallel `HashSet<Vec3i> non_empty`
  tracks the chunks whose `Chunk::empty() == false`, so meshing / render /
  save loops are O(non-empty) instead of O(loaded). The invariant
  `non_empty.contains(c) ⇔ !chunks[c].empty()` is enforced by funnelling
  every `&mut Chunk` borrow through `World::with_chunk_mut(coord, |c| {…})`,
  which re-syncs the side-set on the way out. One grep for `chunks.get_mut`
  shows the only call site is inside `with_chunk_mut`.
- **Empty chunks don't allocate.** `Chunk::empty() ⇔ data.is_none()`. The
  4 KB block array is allocated lazily on first `block_mut` and dropped at
  the end of `init_generate` if no column produced solid content.
- **Async workers off the main thread, owned payloads only.** Two
  `crossbeam-channel`-backed workers: chunk load/save (carries owned bytes
  + a clone of `Arc<sled::Db>`) and meshing (carries an owned
  `MeshInput { coord, padded: Box<[BlockData; 18·18·18]> }` snapshot —
  no chunk reference crosses thread boundaries). Results are re-checked
  via `World::is_loaded(coord)` before being applied.
- **Player lives inside `World`.** `World::update_player` does a `mem::take`
  / reborrow split so the player can read `&self` (via `&dyn BlockView`)
  while being mutably updated.
- **Persistence.** `sled` K/V (replaces LevelDB; pure-Rust, no C++ ABI dep).
  Chunk cells are `bytemuck::cast_slice`'d behind a `NEWC` magic + version
  header. Player save is `bincode` v2 behind `NEWP` magic + version.
- **UI is `egui`.** No bespoke widget DSL; menus are direct `egui` calls.
  `ui::screen` provides a `Screen` trait + `ScreenStack` with `Push / Pop /
  Stay / Exit` transitions.
- **Renderer.** `wgpu` 29.0.1, no separate `Renderer` singleton. Two
  pipelines (opaque + translucent) share `chunk.wgsl`; `Bgra8UnormSrgb`
  surface, `Depth32Float` standard-Z, `Fifo` vsync. Fog and ambient sky
  tint are folded into the chunk shader; there is no deferred G-buffer
  composition pass.
- **Crate dependencies (high-signal).** `wgpu`, `winit` (0.30
  ApplicationHandler), `egui` 0.34 + `egui-winit` + `egui-wgpu`, `cgmath`,
  `glyphon`, `image`, `tracing`, `serde` + `bincode` + `toml`, `sled`,
  `crossbeam-channel`, `rand`, `thiserror`, `bytemuck`.

---

## 3. Feature parity report

### Module-by-module

| C++ module | Rust module | Parity | Notes |
|---|---|---|---|
| `math/{vector,matrix}.ixx` | `src/math/mod.rs` | ✅ Full | `cgmath` re-exports + `Vec3i/f/d`, `Mat4f` aliases. |
| `math/{aabb,euler,frustum}.ixx` | `src/math/{aabb,euler,frustum}.rs` | ✅ Full | True scalar-generic ports. |
| `blocks.ixx` | `src/blocks.rs` | ✅ Full | 19 base blocks; `TextureIndex` folded into `BlockInfo`. The C++ `block_info_registry` / `base_blocks` globals deliberately not ported. |
| `items.ixx` | `src/items.rs` | ✅ Full | `#[repr(C)]` Pod + `merge_into`. |
| `chunks.ixx` | `src/chunks/{mod,generate}.rs` | ✅ Full | Lazy `Box<[BlockData; 4096]>`; `init_generate` ports the column-by-column terrain layering 1:1. `Chunk::empty() ⇔ data.is_none()` invariant added (cleaner than C++'s separate `_empty` flag). |
| `terrain_generation.ixx` + `height_maps.ixx` | `src/terrain_generation.rs` + `src/height_maps.rs` | ✅ Full | Direct port of the noise math. `noise_2d` mixes `seed` via a Wang-style xor-shift-multiply, fixing the C++ bug where `_seed` was unused. |
| `worlds/player.ixx` + `player_impl.cpp` | `src/worlds/player/{mod,save}.rs` | ✅ Full | Physics ported; saves use `bincode` v2 with `NEWP` magic. |
| `worlds/worlds.ixx` | `src/worlds/world/{mod,error,pipeline,store}.rs` | ✅ Full + simplified | C++'s `_chunks` + `_renders` + `chunk_pointer_arrays` collapse into `HashMap<Vec3i, Chunk>` + `HashSet<Vec3i> non_empty`. Async load/save uses `crossbeam-channel` + `sled`. `process_block_updates` runs every sim tick. |
| `worlds/chunk_rendering.cpp` | `src/render/mesh.rs` (CPU) + `src/worlds/chunk_rendering.rs` (GPU) | ✅ Full | 1-D greedy run merging, per-vertex smooth lighting / soft AO, "nice grass" side-face swap. All three behaviours gated on live `MeshOptions`; opaque + translucent pipelines, reversed-Z depth. |
| `worlds/world_rendering.cpp` | `src/game/mod.rs` (`pump_meshing` / draw dispatch) | ✅ Functional | Per-frame draw dispatch over `Game::chunk_meshes`; selection wireframe drawn before egui. |
| `commands.ixx` | `src/commands/{mod,base}.rs` | ✅ Full | All 12 base slash-commands ported; deterministic tab-complete. |
| `globalization.ixx` + `lang/*.lang` | `src/globalization.rs` + `assets/lang/<code>.toml` | ✅ Full | One TOML per language; `get` returns `""` on miss. |
| `particles.ixx` | `src/particles.rs` (sim) + `src/render/particle_render.rs` (GPU) | ✅ Full | Gravity, drag, AABB-collision; billboard pipeline. Per-particle `prev_coord` lerp for smooth sub-tick motion + random `tex_size × tex_size` UV sub-rect per fleck (matches C++ `tcx/tcy = rnd() * (1 - psize)`). |
| `text_rendering.ixx` | `src/text_rendering.rs` | ✅ Functional | Replaced FreeType + hand-rolled atlas with `glyphon`; same call shape from HUD. |
| `textures.ixx` | `src/textures.rs` | ✅ Full | `Atlases::{block_diffuse, block_normal, block_noise, ui_*}` D2-array + 2D textures. `block_diffuse` ships a CPU-generated mipmap chain (full pyramid down to 1×1) so distant chunks anti-alias cleanly. |
| `globals.ixx` (options part) | `src/config.rs` | ✅ Full | Serde TOML at `configs/options.toml`; live-edited by options menu. |
| `globals.ixx` (input/window/runtime) | `src/input.rs` + `src/app.rs` | ✅ Full | Pure-data `InputState`; window state lives on `App`. The ~50 free `export` mutables of `globals.ixx` are intentionally gone. |
| `setup.ixx` | `src/app.rs` (winit wiring) + `src/setup.rs` (tracing init) | ✅ Full | Window/surface bring-up + F11 borderless-fullscreen toggle. |
| `neworld.ixx` (god file) | `src/app.rs` + `src/game/mod.rs` + `src/ui/{hud,inventory}.rs` | ✅ Full + split | Fixed-step 30 Hz accumulator, raycast/break/place, chat, screenshots all present. |
| `menus/main_menu.cpp` | `src/menus/main_menu.rs` | ✅ Full | |
| `menus/world_menu.cpp` | `src/menus/world_menu.rs` | ✅ Full | Refreshes entries every frame. |
| `menus/create_world_menu.cpp` | `src/menus/create_world_menu.rs` | ✅ Full | |
| `menus/game_menu.cpp` | `src/menus/game_menu.rs` | ✅ Full + composes HUD/inventory | |
| `menus/options_menu.cpp` | `src/menus/options_menu.rs` | ✅ Full | FOV / render distance / mouse sens; pushes to render-options / UI-options / language sub-screens like C++. |
| `menus/render_options_menu.cpp` | `src/menus/render_options_menu.rs` | ✅ Full | Smooth lighting + fancy grass + merge-face all wired live (mesh-config change drops every cached chunk mesh and re-marks the loaded set dirty). MSAA picker stored but not yet applied to the wgpu surface. |
| `menus/shader_options_menu.cpp` | `src/menus/shader_options_menu.rs` | 🟡 Stored only | Shadow res / distance / soft shadow / volumetric clouds — values persist into `Config` but Rust has no shadow / volumetric pipelines (Tier 4). |
| `menus/ui_options_menu.cpp` | `src/menus/ui_options_menu.rs` | 🟡 Subset | Font scale yes; `ui_stretch` + `ui_background_blur` stored but not yet applied. |
| `menus/language_menu.cpp` | `src/menus/language_menu.rs` | ✅ Full | Lists every `assets/lang/*.toml` dynamically. |
| `ui/{context,element,layout,render,controls/*}.ixx` | — (replaced by `egui`) | 🟢 By design | Wholesale replacement; `src/ui/{action,hud,inventory,screen}.rs` is Rust-specific glue, not a port. |
| `render/{buffer,texture,framebuffer,program,vertex_array,attrib_*,block_*,image}.ixx` | `src/render/*` | 🟢 By design | Wholesale replacement of the GL RAII layer with `wgpu`. |
| `rendering.ixx` (`Renderer` namespace) | — | 🟡 Replaced | The C++ pass-coordinator singleton has no Rust analog; per-pass dispatch is direct in `Game`. The deferred renderer is not ported — `final.fsh` (641 lines) has no Rust counterpart. |

### Shader parity

| C++ shader | WGSL counterpart | Status |
|---|---|---|
| `default.{vsh,fsh}` | — | Fallback; not needed in wgpu. |
| `ui.{vsh,fsh}` | (egui's own pipeline) | Replaced. |
| `opaque.{vsh,fsh}` + `translucent.{vsh,fsh}` | `shaders/chunk.wgsl` | Combined into one shader with two pipeline configs; deferred G-buffer outputs **not** emitted. |
| `final.fsh` (composition, fog, sky, volumetric clouds, SSR, shadow filter) | partly folded into `chunk.wgsl` | Only fog + ambient sky tint kept; all post-processing dropped. |
| `shadow.{vsh,fsh}` + `debug_shadow.{vsh,fsh}` | — | ❌ Not ported. No shadow maps. |
| `filter.{vsh,fsh}` | — | ❌ Not ported. No post-blur. |
| (none) | `shaders/basic.wgsl` | Bring-up scaffold. |
| (none) | `shaders/particle.wgsl` | Billboard particles (replaces inline GL particle code). |
| (none) | `shaders/selection.wgsl` | Selection-wireframe pass — line list, reversed-Z `Greater`, color-inversion blend (`OneMinusDst` × white = `1 - dst`) so the outline is high-contrast against any backdrop. |

### Bottom line

Mechanical parity (chunk model, world storage, player physics, particles,
commands, i18n, basic rendering) is full, and Tier 2 renderer polish
(smooth lighting, greedy meshing, reversed-Z, BFS light propagation,
random tick, mipmaps, smooth-particle interpolation, in-world selection
wireframe) all shipped. Remaining gaps are the Tier 4 deferred renderer
(shadow maps, G-buffer composition, volumetric clouds, SSR) plus the
shader-options + UI-options panels that those features back. None of the
gaps block end-to-end play.

---

## 4. Roadmap to bring full feature parity

Roughly ordered by reward / effort. Items within a tier are independent and
can be picked up in any order.

### Tier 1 — small wiring fixes (✅ shipped)

- ✅ `process_block_updates` runs from `Game::tick_sim` every tick (also
  drives the BFS light-removal pass that landed in Tier 2).
- ✅ Worldgen `noise_2d` mixes the seed via a 64-bit xor-shift-multiply.
- ✅ F11 toggles borderless fullscreen.
- ✅ Mid-game render-distance updates: `App::apply_config` calls
  `World::set_render_distance` each frame; the height-map cache rebuilds
  and chunks stream in / out over the next few ticks.
- ✅ Block-icon textures: each diffuse-atlas layer is registered as an
  `egui::TextureId` at boot and painted into hotbar / inventory slots.
- ✅ Language menu walks `assets/lang/*.toml` dynamically (native names
  pulled from each file's metadata).

### Tier 2 — medium renderer features (✅ shipped)

- ✅ **Smooth lighting / per-vertex AO.** `ChunkVertex` carries a
  `light: u32` attribute; `mesh_chunk` averages the 4-corner brightness
  using the C++ formula (sky-light exponential + block-light inverse-square
  falloff). The rasterizer interpolates across corners and `chunk.wgsl`
  modulates diffuse by `mix(0.35, 1.0, light)`. Flat-lighting fallback
  (single in-front cell brightness) when `Config::smooth_lighting` is off.
- ✅ **Greedy face merging.** `mesh::mesh_chunk` is a 1-D greedy run
  merger (port of `_merge_face_render_chunk`). Merge axis is +Z for ±X/±Y
  faces, +Y for ±Z faces; the texture sampler is `Repeat` so each per-block
  art square tiles across a merged span. Flat surfaces drop from `S²`
  quads to `S` strips per chunk side. Disabled per-face when
  `Config::merge_face` is off.
- ✅ **Reversed-Z depth.** `OPENGL_TO_WGPU_REVERSED` projection,
  `CompareFunction::Greater`, depth clear 0.0 across chunk + particle +
  selection pipelines.
- ✅ **Light-propagation engine.** `set_block` detects opaque /
  light-emitter transitions and runs `remove_light_bfs` to clear cells
  whose light derived from the source. Cleared cells + independent
  boundary cells re-enter `block_update_queue` so the existing
  max-relaxation pass refloods the region. Sky-light's vertical
  no-falloff is honoured during removal.
- ✅ **Random tick.** `World::random_tick` samples
  `RANDOM_TICKS_PER_CHUNK = 3` cells per non-empty chunk per simulation
  tick, with rules for grass smother (opaque block above → dirt) and
  grass spread (dirt next to grass with no opaque block above → grass).
  `World::drain_updated_chunks` promotes any internal mutation
  (random-tick, BFS light clear, queued block updates) into a chunk
  remesh, so terrain animates without each call site having to dirty
  meshes by hand.

#### Follow-on polish (also shipped)

- ✅ **Live mesh-config.** `MeshOptions{smooth_lighting, merge_face,
  nice_grass, grass_id}` rides on every `MeshInput`; `Game::apply_mesh_config`
  drops the entire `chunk_meshes` map and re-marks the loaded set dirty
  when any flag flips, so the render-options menu acts instantly.
- ✅ **Nice-grass side faces.** Grass blocks sitting on top of grass use
  the grass-top texture for their four side faces (mirrors C++
  `NiceGrass`).
- ✅ **Block-atlas mipmaps.** CPU-generated 2×2 box-average pyramid for
  `block_diffuse` (full chain to 1×1). Sampler is `Nearest` mag/min with
  `Linear` mipmap filter — voxel pixels stay crisp close-up, distant
  chunks anti-alias instead of shimmering.
- ✅ **Particle interpolation + texture flecks.** `Particle` carries a
  `prev_coord` snapshot; `ParticleMesh::rebuild` lerps to `coord` by
  `tick_alpha` so motion is smooth across the 30 Hz / render-rate gap.
  Each fleck samples a `tex_size × tex_size` random sub-rect of the
  source face (matches C++ `tcx/tcy = rnd() * (1 - psize)`).
- ✅ **In-world selection wireframe.** `SelectionPipeline` (own WGSL
  shader, 24-vertex line list) draws into the world depth buffer with
  reversed-Z `Greater` and corners pushed by `EPS = 0.005` so the lines
  clear the block faces. Fragment color is pure white blended with
  `BlendFactor::OneMinusDst` → `out = 1 - dst`, giving a high-contrast
  inverted-color outline against any backdrop. Pass runs before the egui
  pass, so HUD / inventory / pause overlays sit on top.

### Tier 3 — sub-options menus + their backing features

All four sub-menus exist as Rust screens (`render_options_menu.rs`,
`shader_options_menu.rs`, `ui_options_menu.rs`, `language_menu.rs`).
Toggle wiring varies:

- ✅ **Render options menu.** Smooth lighting / fancy grass / merge-face
  drive the live `MeshOptions` snapshot. MSAA picker stored only — the
  surface is single-sampled for now.
- 🟡 **Shader options menu.** Shadow res / shadow distance / soft shadow
  / volumetric-clouds toggles persist into `Config` but have no Rust
  pipelines behind them yet (Tier 4).
- 🟡 **UI options menu.** Font scale yes; `ui_stretch` + `ui_background_blur`
  stored only.
- ✅ **Language menu.** Dynamic list of `assets/lang/*.toml`; switching
  reloads the i18n table on the next frame.

### Tier 4 — large renderer features (weeks each)

- **Shadow maps.** Port `shadow.{vsh,fsh}` to WGSL; add a
  `ShadowPipeline` that renders the scene from the sun's POV into a
  depth texture; sample in `chunk.wgsl` with PCF for soft shadows. Wire
  shadow distance / resolution to the shader-options menu.
- **Deferred G-buffer + composition pass.** Port the largest C++ shader
  (`final.fsh`, 641 lines) — diffuse / normal / material / depth
  attachments from the opaque pass, composited against shadow + sky.
  This is the renderer-architecture rewrite. Without it the other
  Tier 4 items have no place to live.
- **Post-process pipeline.** `filter.{vsh,fsh}` → a generic blur /
  composition pass. Foundation for menu background blur and the
  composition pass above.
- **Volumetric clouds.** Raymarched in the composition pass.
- **SSR (screen-space reflections).** Reads the G-buffer in the
  composition pass.

### Out of scope (intentionally not ported)

- `chunk_pointer_arrays.ixx` — the sliding-window `Chunk*` cache. The
  per-access hash on `HashMap<Vec3i, _>` is cheap; the cache earned its
  complexity in C++ from defending against pointer aliasing the Rust
  port doesn't have.
- C++ `globals.ixx` mutable bag — split into `Config`, `InputState`,
  `App`-fields; not coming back.
- C++ `ui/*.ixx` widget DSL — `egui` covers the surface area.
- C++ `render/*.ixx` GL RAII layer — `wgpu` covers the surface area.
- C++ on-disk formats (LevelDB chunk store, `reinterpret_cast` player
  save, INI options, `lang/keys.lk` + `lang/*.lang`) — replaced by
  `sled` + `bincode` + TOML. No migration tool; existing C++ worlds
  aren't loadable.

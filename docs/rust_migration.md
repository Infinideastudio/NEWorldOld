# Rust Port — Design and Feature Parity

This document describes the Rust port of NEWorld and how it relates to the
C++23 original (preserved under [`old/`](../old/) for diffing). It covers:

1. The shape of the C++ codebase as it stands.
2. The shape of the Rust codebase as it stands.
3. A module-by-module feature-parity report.
4. A roadmap to close the remaining gaps.

The migration is well past the bring-up phase — all six of the original
implementation groups (`[A]`–`[F]`) have shipped and the binary is end-to-end
playable. The remaining gap with C++ is cosmetic: a Cook-Torrance BRDF
(needs metallic/roughness in the G-buffer).

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

Single Cargo crate, edition 2024, Rust 1.95+. ~21 K LoC of Rust + ~1.9 K lines
of WGSL. All graphics / windowing / audio deps are pure-Rust crates; no
external system libraries. License: CC0-1.0.

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
├── commands/{mod,base}.rs         ↔ commands.ixx (11 base commands; the C++
│                                  /suicide is gone since player health was
│                                  removed)
├── math/{mod,aabb,euler,frustum}.rs   ↔ math/*.ixx
│
├── game/                          in-process game orchestrator
│   ├── mod.rs                     tick_render / tick_sim / pump_meshing /
│   │                              break / place / chat dispatch
│   ├── camera.rs                  passive view; mirrors player
│   ├── raycast.rs                 Amanatides–Woo voxel DDA
│   ├── hud.rs                     crosshair / debug / chat / selection
│   └── inventory.rs               4×10 grid + always-on hotbar + mouse pickup
│
├── menus/                         ↔ menus/*.cpp
│   ├── main_menu.rs / world_menu.rs / create_world_menu.rs
│   ├── options_menu.rs            consolidated; see §3
│   ├── game_menu.rs               composes HUD + inventory + pause
│   ├── action.rs                  WorldActionQueue (cross-screen lifecycle)
│   └── screen.rs                  Screen trait + ScreenStack
│
├── render/                        wgpu replacement for old/src/render/
│   ├── context.rs                 Gfx (instance/adapter/device/queue/surface)
│   ├── basic_pipeline.rs          bring-up scaffold
│   ├── depth.rs                   DepthTarget (reversed-Z Depth32Float)
│   ├── egui_renderer.rs           egui ↔ wgpu bridge
│   ├── mesh.rs                    CPU per-face culled meshing
│   ├── mesh_pipeline.rs           off-thread mesh worker
│   ├── particle_render.rs         billboard particle pipeline
│   ├── screenshot.rs              surface readback → PNG
│   ├── selection.rs               wireframe-outline pipeline
│   ├── underwater.rs              underwater overlay pipeline
│   ├── gbuffer.rs                 deferred G-buffer (diffuse / normal /
│   │                              material / depth)
│   ├── composition.rs             composition pipeline (final.fsh port)
│   ├── shadow.rs                  shadow map (Depth32Float + comparison)
│   ├── shadow_pipeline.rs         sun-POV depth fill pipeline
│   ├── debug_shadow_pipeline.rs   F3+M shadow-atlas overlay
│   └── uniforms.rs                FrameUniforms / ModelUniforms / FilterUniforms
│
├── ui/                            Bespoke Flutter-style layout engine
│   │                              (atop egui's painting layer)
│   ├── layout.rs                  Element trait + Constraint / Size / Point
│   ├── mod.rs                     `show` entry point — every menu including
│   │                              the in-game pause overlay goes through it
│   │                              (transparent CentralPanel + theme-aware
│   │                              dimmer scrim, painted under the widgets)
│   └── widgets/                   Aligned, Padding, Sizer, Spacer, Flex,
│                                  ScrollView, Label, Button, SelectButton,
│                                  Slider, TextEdit, Image (with `BoxFit`)
│
└── worlds/
    ├── chunk_rendering.rs         ↔ worlds/chunk_rendering.cpp (ChunkMesh
    │                              + ChunkPipeline)
    ├── player/{mod,save}.rs       ↔ player.ixx + player_impl.cpp
    └── world/                     ↔ worlds.ixx
        ├── mod.rs                 incl. LoadedCore concentric ring cache
        ├── error.rs               WorldError (thiserror)
        ├── pipeline.rs            async chunk load/save worker
        └── store.rs               TilesStore (sled)

shaders/
├── basic.wgsl                     bring-up scaffold
├── chunk.wgsl                     opaque + translucent G-buffer +
│                                  basic-mode forward (shared shader)
├── default.wgsl                   reference port of the C++ default.fsh
├── shadow.wgsl                    sun-POV depth fill (leaf wave + fisheye)
├── debug_shadow.wgsl              F3+M overlay (8-step binary search)
├── composition.wgsl               final.fsh port (lambert + PCF + SSR +
│                                  Gerstner waves + clouds + ACES)
├── filter.wgsl                    separable Gaussian (drives menu blur)
├── menu_background.wgsl           rotating sky-cube backdrop for menus
├── particle.wgsl                  billboards
├── selection.wgsl                 wireframe-outline (color-inversion blend)
└── underwater.wgsl                underwater tint quad
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
- **UI is a bespoke Flutter-style layout engine on top of egui.** The
  `egui::Context` is still the painting + input host, but `src/ui/`
  defines its own `Element` trait, `Constraint` / `Size` / `Point`
  geometry, and a widget set (Padding, Sizer, Spacer, Aligned, Flex,
  ScrollView, Label, Button, SelectButton, Slider, TextEdit, `Image`
  with the C++ `BoxFit` shape ported via `apply_box_fit`). Menus build
  trees of these widgets; egui draws them through a single entry point
  (`ui::show` — transparent CentralPanel + theme-aware dimmer scrim
  painted under the widgets) used by every menu including the in-game
  pause overlay. `menus::screen` provides a `Screen` trait + `ScreenStack`
  with `Push / Pop / Stay / Exit` transitions.
- **Theme is a live `Config::dark_theme` toggle**, defaulting to light
  so the menus read against the bright sky-cube panorama. `App::apply_config`
  re-applies `Visuals::dark()` / `Visuals::light()` to the egui context
  every frame and forces `override_text_color = widgets.inactive.fg_stroke.color`
  so labels, slider value displays, and button text all match.
- **`Config::ui_scale`** drives `egui::Context::set_pixels_per_point`
  (combined with the OS DPI) so layout + font rendering scale uniformly.
  The slider lives in the options menu and only commits to `Config` on
  Back/Save (no re-layout while dragging).
- **Renderer.** `wgpu` 29.0.1, no separate `Renderer` singleton. Two
  modes share `chunk.wgsl`: a basic forward path (`fs_main_forward`) and
  a deferred G-buffer path (`fs_main` MRT). `Bgra8UnormSrgb` surface,
  `Depth32Float` reversed-Z (near=1, far=0, `CompareFunction::Greater`,
  depth clear 0.0). Vsync is a live `Config` toggle, not hardcoded.
  Advanced mode runs shadow → G-buffer → composition (`final.fsh`
  port); basic mode runs a single forward opaque pass with fog and
  ambient sky tint folded into the chunk shader.
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
| `worlds/chunk_rendering.cpp` | `src/render/mesh.rs` (CPU) + `src/worlds/chunk_rendering.rs` (GPU) | ✅ Full | 1-D greedy run merging, per-vertex smooth lighting / soft AO, "nice grass" side-face swap, per-fragment **normal mapping** via `block_normal` atlas + face-derived TBN. All toggleable behaviours gated on live `MeshOptions`; opaque + translucent G-buffer pipelines + opaque/translucent forward (basic) pipelines, reversed-Z depth. |
| `worlds/world_rendering.cpp` | `src/game/mod.rs` (`pump_meshing` / draw dispatch) | ✅ Functional | Per-frame draw dispatch over `Game::chunk_meshes`; selection wireframe drawn before egui. `Game::record_world_pass` branches on `Config::advanced_render`: advanced path runs shadow → G-buffer → composition; basic path runs a single forward opaque pass via `ChunkPipeline::begin_opaque_forward`. |
| `commands.ixx` | `src/commands/{mod,base}.rs` | ✅ Full | 11 of 12 C++ slash-commands ported; deterministic tab-complete. `/suicide` is gone since the player-health system was removed. |
| `globalization.ixx` + `lang/*.lang` | `src/globalization.rs` + `assets/lang/<code>.toml` | ✅ Full | One TOML per language; `get` returns `""` on miss. |
| `particles.ixx` | `src/particles.rs` (sim) + `src/render/particle_render.rs` (GPU) | ✅ Full | Gravity, drag, AABB-collision; billboard pipeline. Per-particle `prev_coord` lerp for smooth sub-tick motion + random `tex_size × tex_size` UV sub-rect per fleck (matches C++ `tcx/tcy = rnd() * (1 - psize)`). |
| `text_rendering.ixx` | `src/text_rendering.rs` | ✅ Functional | Replaced FreeType + hand-rolled atlas with `glyphon`; same call shape from HUD. |
| `textures.ixx` | `src/textures.rs` | ✅ Full | `Atlases::{block_diffuse, block_normal, block_noise, splash, title, background_cube}` D2-array / 2D / cube textures. `block_diffuse` ships a CPU-generated mipmap chain (full pyramid down to 1×1) so distant chunks anti-alias cleanly. The 6 `background_*.png` faces stack into a real cubemap consumed by the menu-background pipeline. The unused `select.png` / `unselect.png` C++ atlases were dropped. |
| `globals.ixx` (options part) | `src/config.rs` | ✅ Full | Serde TOML at `configs/options.toml`; live-edited by options menu. |
| `globals.ixx` (input/window/runtime) | `src/input.rs` + `src/app.rs` | ✅ Full | Pure-data `InputState`; window state lives on `App`. The ~50 free `export` mutables of `globals.ixx` are intentionally gone. |
| `setup.ixx` | `src/app.rs` (winit wiring) + `src/setup.rs` (tracing init) | ✅ Full | Window/surface bring-up + F11 borderless-fullscreen toggle. |
| `neworld.ixx` (god file) | `src/app.rs` + `src/game/mod.rs` + `src/ui/{hud,inventory}.rs` | ✅ Full + split | Fixed-step 30 Hz accumulator, raycast/break/place, chat, screenshots all present. |
| `menus/main_menu.cpp` | `src/menus/main_menu.rs` | ✅ Full | The 256-px banner row paints the `title.png` atlas through the new `Image` widget (bilinear-filtered, `BoxFit::Contain`) instead of a "NEWorld" text label. |
| `menus/world_menu.cpp` | `src/menus/world_menu.rs` | ✅ Full | Refreshes entries every frame. |
| `menus/create_world_menu.cpp` | `src/menus/create_world_menu.rs` | ✅ Full | |
| `menus/game_menu.cpp` | `src/menus/game_menu.rs` | ✅ Full + composes HUD/inventory | When unpaused the screen draws HUD + inventory overlays; when paused it renders the pause column through `ui::show` (same chrome as every other menu — the C++-style modal `Window` was retired) with HUD/inventory suppressed. The live world keeps rendering behind because the world pass runs in `app.rs` regardless of menu state. |
| `menus/options_menu.cpp` | `src/menus/options_menu.rs` | ✅ Full | FOV / render distance / mouse sens / UI scale slider / dark-theme toggle; pushes to render-options / language sub-screens. UI scale only commits on Back/Save so dragging doesn't re-layout the whole menu in real time; theme commits immediately. (The C++ UI-options sub-screen was dropped — see below.) |
| `menus/render_options_menu.cpp` | `src/menus/render_options_menu.rs` | ✅ Full | Smooth lighting + fancy grass + merge-face all wired live (mesh-config change drops every cached chunk mesh and re-marks the loaded set dirty). MSAA picker dropped — every wgpu pipeline is single-sampled. |
| `menus/shader_options_menu.cpp` | `src/menus/shader_options_menu.rs` | ✅ Full | Every toggle is live: advanced rendering / shadow res / shadow distance drive `Game::apply_shadow_config`; soft shadow / volumetric clouds / ambient occlusion drive composition `override` constants and rebuild only on actual change. |
| `menus/ui_options_menu.cpp` | — (removed) | 🟢 By design | The standalone sub-menu is gone. PPI stretch and background blur were never wired and have no Rust counterpart. UI scale survived the cull and reappears as a slider on the top-level options menu (`Config::ui_scale`); a `Config::dark_theme` toggle replaces the C++ "background blur" row in the same column. |
| `menus/language_menu.cpp` | `src/menus/language_menu.rs` | ✅ Full | Lists every `assets/lang/*.toml` dynamically. |
| `ui/{context,element,layout,render,controls/*}.ixx` | `src/ui/` (Flutter-style layout engine on egui) | 🟢 By design | The C++ `View/Element/Builder` + `Row/Column/Stack/Sizer/Padding` + `Button/Slider/TextBox/ImageBox/ScrollView` shape was rebuilt natively in Rust on top of egui's painting + input. HUD / inventory / screen-stack glue lives in `src/game/` and `src/menus/`. |
| `render/{buffer,texture,framebuffer,program,vertex_array,attrib_*,block_*,image}.ixx` | `src/render/*` | 🟢 By design | Wholesale replacement of the GL RAII layer with `wgpu`. |
| `rendering.ixx` (`Renderer` namespace) | `src/render/{gbuffer,composition,shadow,shadow_pipeline,debug_shadow_pipeline,selection,underwater,menu_background}.rs` + `src/game/mod.rs` | ✅ Full | C++ pass-coordinator singleton split across the `render` module; per-pass dispatch is direct in `Game::record_world_pass`. Advanced-mode `final.fsh` is fully ported (composition.wgsl). Out-of-game menu background runs a slowly-rotating sky cube (Minecraft-style yaw + sinusoidal pitch, math in WGSL) plus a 2-pass Gaussian blur via `menu_background.rs`; the in-game pause menu re-uses the same `ui::show` chrome but draws over the live world instead of the cube. |

### Shader parity

| C++ shader | WGSL counterpart | Status |
|---|---|---|
| `default.{vsh,fsh}` | `shaders/default.wgsl` | ✅ Ported (standalone). The basic-mode pipeline reuses `chunk.wgsl::fs_main_forward` rather than `default.wgsl` — same shading math (smooth-light × diffuse texel) but using the shared `ChunkVertex` layout. `default.wgsl` is kept for reference. |
| `ui.{vsh,fsh}` | (egui's own pipeline) | 🟢 Replaced by design. |
| `opaque.{vsh,fsh}` + `translucent.{vsh,fsh}` | `shaders/chunk.wgsl` | ✅ Ported. Two fragment entry points: `fs_main` writes the G-buffer (opaque MRT pass) and `fs_main_forward` writes the surface for the post-composition translucent pass. |
| `final.fsh` (composition, fog, sky, volumetric clouds, SSR, shadow filter) | `shaders/composition.wgsl` | ✅ Ported. Sun lambert + ambient, 4-tap shadow PCF, distance fog into directional sky, **screen-space reflection + Schlick fresnel** for water / ice / iron, **7-octave Gerstner water waves**, **inside-water TIR heuristic** (`smoothstep(0,1,sin²θ)` per the C++ heuristic), ACES tonemap. Reflected pixels go through the same `shade_world_pixel` helper the primary view uses, so SSR fragments get full lambert + shadow + fog + horizon-fade-alpha. Optional features (SOFT_SHADOW / VOLUMETRIC_CLOUDS / AMBIENT_OCCLUSION) gate on WGSL `override` constants — naga folds them and DCEs disabled branches. Skipped: full Cook-Torrance BRDF (collapsed to Lambert since the G-buffer doesn't carry metallic / roughness). |
| `shadow.{vsh,fsh}` | `shaders/shadow.wgsl` | ✅ Ported + live pipeline. Includes leaf wave + fisheye warp; depth-only output (wgpu allows depth-only render passes, so the C++ debug color attachment was dropped). Driven by `src/render/shadow_pipeline.rs`. |
| `debug_shadow.{vsh,fsh}` | `shaders/debug_shadow.wgsl` | ✅ Ported + live pipeline. Mirrors the C++ 8-step binary-search of `textureSampleCompare` to recover the stored depth. Driven by `src/render/debug_shadow_pipeline.rs`; toggled by **F3+M** while advanced rendering is on. |
| `filter.{vsh,fsh}` | `shaders/filter.wgsl` | ✅ Ported + live pipeline. Separable Gaussian blur with the C++ `FilterUniformBlock` layout. Driven by `src/render/menu_background.rs` for the out-of-game menu background blur (horizontal then vertical pass). |
| (none) | `shaders/basic.wgsl` | Bring-up scaffold. |
| (none) | `shaders/particle.wgsl` | Billboard particles (replaces inline GL particle code). |
| (none) | `shaders/selection.wgsl` | Selection-wireframe pass — line list, reversed-Z `Greater`, color-inversion blend (`OneMinusDst` × white = `1 - dst`) so the outline is high-contrast against any backdrop. |
| (none) | `shaders/underwater.wgsl` | Underwater overlay tint (full-screen quad). |
| (none) | `shaders/menu_background.wgsl` | Slowly-rotating skybox cube sampled from `Atlases::background_cube`. Yaw + sinusoidal pitch math runs in the vertex stage from a `time_secs` uniform; the cube vertices project rotated to screen but sample the cubemap by their *un-rotated* direction (otherwise every screen pixel normalises to the same camera ray and the panorama freezes). Output is fed through `filter.wgsl` to produce the blurred out-of-game menu background. |

### Bottom line

Full parity in mechanics (chunk model, world storage, player physics,
particles, commands, i18n) and in both **basic** and **advanced**
rendering modes. Advanced mode covers sun lambert, 4-tap shadow PCF,
fisheye-warped sun-POV depth atlas, distance fog with directional sky,
ACES tonemap, **screen-space reflections + Schlick fresnel** for
water/ice/iron, **7-octave Gerstner water waves**, **inside-water TIR
heuristic**, **volumetric clouds** (with separate raymarches for
primary view and SSR sky-reflection), **SSAO**, **soft-shadow toggle**,
and per-fragment **normal mapping** via a face-derived TBN. All
optional features gate on WGSL `override` constants — naga folds them
and DCEs disabled branches, the same zero-cost-when-off semantics as
C++ `#ifdef`. Remaining gaps are cosmetic / architectural:
Cook-Torrance BRDF (G-buffer would need metallic/roughness channels),
that's it.

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
- ✅ **Light-propagation engine.** Same single-pass max-relaxation as
  C++ `worlds.ixx::update_block` — `set_block` writes the id and queues
  a `block_update_queue` entry; `process_block_updates` drains the queue
  every sim tick, recomputing each cell's light as `max(neighbours) - 1`
  (with the `+Y sky=15 → no falloff` special case and the glowstone /
  lava emit-15 override). The transmission rule keys on the
  `opaque` / `translucent` flags rather than `solid`: non-opaque
  non-translucent blocks (air, **glass, leaf**) take the air path
  (skylight column passthrough plus `-1` diffuse falloff) while
  non-opaque translucent blocks (water, lava, ice) keep the `-1`
  falloff with no skylight fast-path — so a glass roof or leaf canopy
  no longer black-shadows the cells below it. Removing a source
  converges over a few ticks rather than instantly; deliberate, since
  a future block-update rewrite-system rework will subsume this pass.
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

The C++ build had four sub-menus; the Rust port has three
(`render_options_menu.rs`, `shader_options_menu.rs`,
`language_menu.rs`). The C++ UI options sub-menu was dropped — PPI
stretch and background blur have no Rust counterpart; UI scale and a
new dark / light theme toggle survived the cull and live on the
top-level options menu instead (`Config::ui_scale` and
`Config::dark_theme`).

- ✅ **Render options menu.** Smooth lighting / fancy grass / merge-face
  drive the live `MeshOptions` snapshot. MSAA was dropped from `Config`
  — every wgpu pipeline is single-sampled.
- ✅ **Shader options menu.** Every toggle is now live: shadow res /
  shadow distance / advanced rendering drive
  `Game::apply_shadow_config`; soft shadow / volumetric clouds /
  ambient occlusion drive composition `override` constants, with the
  pipeline rebuilt only on actual change.
- ✅ **Language menu.** Dynamic list of `assets/lang/*.toml`; switching
  reloads the i18n table on the next frame.
- ✅ **Top-level options additions.** `Config::ui_scale` slider (commits
  on Back/Save so dragging doesn't re-layout in real time) drives
  `egui::Context::set_pixels_per_point` for both layout and font
  rendering. `Config::dark_theme` toggle commits immediately and pushes
  fresh `Visuals::dark()` / `Visuals::light()` into the context;
  `override_text_color` is forced to the button-text colour so labels
  and slider values match button text on either theme.

### Tier 4 — deferred renderer (✅ shipped)

The deferred renderer architecture and every advanced-mode feature
from `final.fsh` are ported. `Config::advanced_render` is the master
switch: off → forward pipeline (basic mode), on → G-buffer + shadow +
composition (advanced mode).

- ✅ **G-buffer.** `src/render/gbuffer.rs` mirrors the C++
  `Renderer::Deferred` framebuffer: `Rgba16Float` diffuse (HDR + alpha-
  blendable for the translucent G-buffer pass — `Rgba32Float` works in
  GL but wgpu rejects blending it without the `Float32Blendable`
  feature), `Rgba8Unorm` normal (`(n+1)/2` encoded), `Rgba8Unorm`
  material (16-bit block id encoded as 2 bytes — `encode_u16` /
  `decode_u16` ported verbatim), and `Depth32Float` reversed-Z depth.
- ✅ **Frame uniforms.** `FrameUniforms` carries `inv_view_proj`,
  `shadow_view_proj`, `render_distance`, `shadow_params`
  (`(resolution, distance, fisheye_factor, inside_water)`), and the
  `player_coord_int / mod / frac` triplet (C++ "repeat trick" coords
  for SSR / volumetric clouds). Total 448 B; `_pad_scalars: vec2`
  16-aligns `shadow_params` for the WGSL uniform-address-space rule.
- ✅ **Shadow pass.** `src/render/shadow_pipeline.rs` drives a
  depth-only chunk pass that fills `ShadowMap` (`Depth32Float` +
  `GreaterEqual` comparison sampler — matches C++
  `set_depth_compare_mode(GEQUAL)`) from the sun's POV every frame.
  Reuses `ChunkVertex` so `ChunkMesh` opaque buffers draw straight
  through. Reversed-Z `Greater` compare, `0.0` clear, no culling
  (mirrors C++ `glDisable(GL_CULL_FACE)` in `StartShadowPass`).
  Resolution + distance live-config'd by `Game::apply_shadow_config`;
  `shadow_view_proj` is built from `look_to_rh(player_pos, -sun_dir,
  up)` × wgpu reversed-Z ortho.
- ✅ **G-buffer chunk passes.** Opaque pass writes MRT REPLACE.
  Translucent G-buffer pass alpha-blends water / ice / leaves into the
  existing opaque diffuse (depth-write enabled so SSR sees the water
  surface; mirror of C++ `glEnable(GL_BLEND)` around
  `StartTranslucentPass`). Water / ice get `texel.a = 0.02` forced in
  `chunk.wgsl::fs_main_translucent` — direct port of C++
  `translucent.fsh`.
- ✅ **Composition pass — full `final.fsh` port.** Lambert + ambient,
  4-tap shadow PCF (`textureSampleCompareLevel` so the call works
  under non-uniform control flow), distance fog into directional sky,
  ACES tonemap. Reflected pixels go through the same
  `shade_world_pixel` helper the primary view uses, so SSR fragments
  receive full lambert + shadow + fog + horizon-fade-alpha. SSR
  itself is the C++ raymarch + Schlick fresnel for water / ice / iron;
  the cloud raymarch is reused for both the primary view and the SSR
  sky-reflection (water-surface origin, half-quality, separate
  `center` for the horizon-fade — mirrors the C++ comment-out at
  `final.fsh:598`). Optional features (SOFT_SHADOW / VOLUMETRIC_CLOUDS
  / AMBIENT_OCCLUSION) gate on WGSL `override` constants and
  `CompositionPipeline::rebuild_with_features` only rebuilds when a
  flag actually changes.
- ✅ **Water waves.** 7-octave Gerstner sum in `composition.wgsl`
  (`calc_wave_normal`) — direct port of C++ `final.fsh:394`. Time
  drives wave phase (`frame.time` is real seconds; the C++ `/30`
  converted ticks to seconds, dropped here since we're already in
  seconds — wave period is now ~2.3 s for an 8-block swell, vs. ~70 s
  before the fix). Wave sample point uses
  `view_relative + player_coord_mod + player_coord_frac` so the trig
  math stays in a precision-friendly range.
- ✅ **Inside-water TIR heuristic.** `shadow_params.w` carries the
  "camera is inside water" bit (set by `Game::write_frame_uniforms`
  whenever the eye block is water). The composition shader flips
  `cos_theta`, swaps the reflection base from sky → `vec3(0.1)`, and
  uses `smoothstep(0, 1, sin²θ)` instead of Schlick — mirrors the
  C++ `inside` branch at `final.fsh:610`. A TODO is parked in the
  shader for a better TIR approximation if one comes along.
- ✅ **Normal mapping.** `block_normal` atlas now bound at
  `chunk.wgsl` group 1 binding 2; per-fragment sampled in `fs_main` /
  `fs_main_translucent`. World-space normal computed via
  `face_tbn(face) * decode(texel)` — TBN is right-handed by
  construction (`B = cross(N, T)`); per-face tangent is hand-picked
  to roughly align with our V-flipped UV convention. Applies to both
  basic forward and advanced G-buffer paths.
- ✅ **Per-face directional dimming.** `mesh::apply_face_dim` is
  gated on `MeshOptions::advanced_render`: applied in **basic** mode
  (mirrors C++ `if (!AdvancedRender) col = col * N / 10`), skipped in
  **advanced** mode (otherwise composition's lambert would
  double-darken side faces). Mesh rebuilds automatically on toggle.
- ✅ **Underwater overlay.** Forward overlay quad samples the water
  texture and alpha-blends over the surface when the camera's eye
  block is water — matches the C++ `neworld.ixx:783` overlay.
- ✅ **F3+M debug shadow overlay.** `src/render/debug_shadow_pipeline.rs`
  draws the shadow depth atlas as a square in the top-right of the
  screen via `debug_shadow.wgsl`'s 8-step binary search. Toggled by
  F3+M while advanced rendering is on; auto-clears when advanced
  flips off.
- ✅ **All shaders ported.** `default.wgsl`, `chunk.wgsl` (= opaque +
  translucent + opaque-forward + translucent-forward), `composition.wgsl`,
  `shadow.wgsl`, `debug_shadow.wgsl`, `filter.wgsl`, `particle.wgsl`,
  `selection.wgsl`, `underwater.wgsl`, `menu_background.wgsl`. Every
  shader has a live pipeline.

### Remaining gaps (cosmetic / architectural)

- **Cook-Torrance BRDF.** Composition uses pure Lambert + ambient.
  The C++ build does Cook-Torrance with `metallic = 0, roughness = 1`,
  which collapses to Lambert anyway for the diffuse term; the specular
  term is negligible at roughness 1. A real BRDF needs metallic /
  roughness channels in the G-buffer (currently only `block_id` is
  encoded into `material`).

### WGSL ↔ GLSL / wgpu ↔ OpenGL mismatch report

Issues encountered during the Tier 4 shader ports. Kept here as a
reference catalog for any future shader work.

#### Texture / sampler model

1. **Combined samplers split into texture + sampler.** GLSL
   `sampler2DArray u_diffuse` ↔ WGSL `texture_2d_array<f32>` plus a
   separate `sampler` binding, called via `textureSample(tex, samp,
   uv, layer)`. Every chunk shader needs the explicit pair.
2. **Shadow comparison samplers.** GLSL `sampler2DArrayShadow` ↔ WGSL
   `texture_depth_2d` + `sampler_comparison`, sampled via
   `textureSampleCompare`. The sampler must declare `compare:` at
   creation; binding-type mismatch is a runtime validation error.
3. **`texelFetch` is non-filtering.** WGSL equivalent is
   `textureLoad(tex, ivec2, level)`. Used in the composition shader
   for G-buffer reads where filtering would average across material
   boundaries.
4. **Sloppy GLSL `sampler2DArray` on a 2D texture.** The C++
   `filter.fsh` declares `sampler2DArray u_buffer` but binds a 2D
   texture. OpenGL drivers permit this (sampling layer 0); WGSL is
   strict. The Rust port declares `texture_2d<f32>` and drops the
   `vec3(uv, 0.0)` z-component.

#### Coordinate conventions

5. **NDC depth range.** GLSL `[-1, 1]`, wgpu/Vulkan/D3D `[0, 1]`.
   Patched at the camera level via `OPENGL_TO_WGPU_REVERSED`
   (`src/game/camera.rs`). The shadow pass's `shadow_view_proj` applies
   the same correction.
6. **NDC Y direction is +Y up in both,** but **texture-space Y
   differs**: GLSL `t = 0` is at the bottom of the texture, WGSL
   `t = 0` is at the top. Already handled in `mesh.rs` (`FACE_UVS` is
   V-flipped vs the C++ `tex_coords` table); every full-screen quad's
   `out.uv = vec2(p.x*0.5+0.5, 1.0 - (p.y*0.5+0.5))` re-flips for the
   composition / debug / filter passes.
7. **`gl_FragCoord` ↔ `@builtin(position)` in fragment.** Same
   semantics (`(screen_x, screen_y, depth, 1/clip_w)`) once the Y-flip
   is accounted for.

#### Shader language differences

8. **`centroid in` ↔ `@interpolate(perspective, centroid)`** —
   straight syntactic replacement.
9. **`flat in` ↔ `@interpolate(flat)`** — same.
10. **`uvec3 a_color` ↔ `@location(N) color: vec3<u32>`** with
    `Uint32x3` vertex format. The host-side `wgpu::VertexFormat`
    choice has to match the shader's input type bit-for-bit (no
    implicit promotion).
11. **No `inverse()` builtin in WGSL.** Compute on the host (we use
    `cgmath::Matrix4::invert` in `Game::write_frame_uniforms`) and
    upload as `inv_view_proj`.
12. **No C-style `#define` / preprocessor in WGSL.** The C++ build
    conditionally compiles the same shader with `MERGE_FACE`,
    `SOFT_SHADOW`, `VOLUMETRIC_CLOUDS`, `AMBIENT_OCCLUSION` macros
    (`rendering.ixx::init_pipeline`). Rust port options:
    (a) string-substitute before `create_shader_module`,
    (b) WGSL `override` constants with pipeline-creation override values,
    (c) ship multiple shader files. We use (c) for the basic-vs-deferred
    chunk split (`chunk.wgsl` MRT entry vs `fs_main_forward`) and (b)
    for the composition-shader feature flags — naga folds
    pipeline-creation overrides and dead-code-strips disabled branches,
    same zero-cost-when-off semantics as `#ifdef`.
13. **Float-bounded loops** (`for (float i = -radius; i <= radius;
    i += step)`) work in both. WGSL's `loop {}` form is more
    idiomatic — used in `filter.wgsl`.
14. **Uniform-block layout rules differ subtly from std140.**
    WGSL's uniform-address-space alignment requires `vec4`-aligned
    `mat4` / `vec4` fields; trailing scalars need explicit padding to
    land any following `vec4` on a 16-byte boundary. Caught one
    offset mismatch in `FrameUniforms` — fixed with
    `_pad_scalars: vec2<f32>` between `render_distance` and
    `shadow_params`.

#### Render-target / blending

15. **`Rgba32Float` is not blendable in wgpu by default.** GL freely
    allows blending on `RGBA32F`; wgpu requires the
    `Float32Blendable` feature. Once we needed water to alpha-blend
    into the G-buffer for SSR, the diffuse target was switched to
    `Rgba16Float` — half-float HDR that's blendable out of the box,
    with plenty of range for HDR sun radiance. Basic-mode translucent
    still uses the surface-format forward pipeline (the split is now
    a basic-vs-advanced choice rather than a format limitation).
16. **Depth-only render passes are allowed in wgpu** but the C++
    `Framebuffer` wrapper requires a non-empty color attachment
    list. Result: the C++ shadow framebuffer has a never-sampled
    `RGBA8_UNORM` color attachment; the Rust port omits it.
17. **MRT clear values for uint formats.** wgpu's `wgpu::Color {
    r, g, b, a }` is typed as `f64`; for `R*Uint` formats the
    validator interprets the components as integer values clamped to
    the format's max. Clear value `0.0` works correctly. Avoided
    entirely by switching `material` from `R16Uint` → `Rgba8Unorm`
    for C++ parity (`encode_u16` / `decode_u16` ported verbatim).
18. **Vertex `u8` / `i8` packed attributes** in C++
    (`Color: Vec3u8`, `Tangent: Vec3i8`) require
    `wgpu::VertexFormat::Unorm8x4` / `Snorm8x4` and 4-byte component
    padding. Our `ChunkVertex` doesn't carry tangents yet; whenever
    it does we'll need 4-component packed types since wgpu doesn't
    have `*x3` 8-bit formats.

#### Vertex layout

19. **`a_color: uvec3`** in C++ holds per-channel smooth-light
    brightness (always uniform across r/g/b in practice). The Rust
    port stores monochrome brightness in `light: u32`. Functionally
    equivalent; the WGSL shader extracts via
    `f32(in.light & 0xFFu) / 255.0` rather than `vec3(a_color) / 255.0`.
20. **Texture array layer in `tex_coord.z`** vs explicit `layer:
    u32` attribute. Adapted in every shader that consumes chunk
    vertices.
21. **`block_id` carried per-vertex.** Added to `ChunkVertex`
    (vertex stride 32 → 36 B) so the chunk fragment shader can
    write the material into the G-buffer per-pixel — same role as
    C++ `a_block_id` (location 5) in `opaque.vsh` /
    `translucent.vsh`.

#### Reversed-Z subtleties

22. **Shadow comparison direction.** C++ uses `GL_GEQUAL` for
    reversed-Z shadow. With `sampler_comparison` +
    `GreaterEqual`, `textureSampleCompare(tex, samp, uv, ref)`
    returns `1.0` when the stored depth is `≥ ref` (the test point
    is at or in front of the closest occluder, i.e. lit). This is
    what the live shadow pass and `debug_shadow.wgsl` binary search
    use.
23. **Fisheye warp commutes through the perspective divide only
    because xy is divided post-w-divide.** `shadow.vsh` does the
    divide explicitly before warping. Standard rasterization
    expects clip-space coords (pre-divide); writing post-divide
    coords with `w=1` is a load-bearing detail the WGSL port
    preserves.

#### Bindings / pipeline state

24. **Bind-group-vs-pipeline-layout reflection.** wgpu validates
    that every binding declared in the shader has a matching
    `BindGroupLayoutEntry`. Solution: declare all shadow / noise
    bindings in the composition pipeline's `aux_layout` even when
    unused at runtime.
25. **Static bindings can get dead-stripped** if the shader never
    reads them — wgpu's reflection then complains about a layout
    mismatch. The composition shader does a zero-scale
    `textureSampleCompare + textureSampleLevel` to anchor the
    bindings without affecting output.

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

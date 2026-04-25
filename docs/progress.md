# Rust port — progress log

This document tracks the state of the Rust port at `rs/`. The migration plan
itself lives in [`rust_migration.md`](rust_migration.md); this file records
which sections of the plan have shipped, what their shape ended up being, and
what's still open.

## Current state

* Crate layout: single `rs/` Cargo crate, edition 2024, Rust 1.95+.
* License: `CC0-1.0`.
* Lints: workspace clippy pedantic, `unsafe_code = "deny"`. CI-style check is
  `cargo clippy --all-targets -- -D warnings`.
* Tests: 149 lib unit tests + 2 integration tests under `tests/smoke.rs`,
  all passing across four consecutive runs of `cargo test`.
* The binary opens a 1280×720 window, opens the title menu on top of an
  asynchronously-streaming 7×7×7 chunk world, and once the user clicks
  "Back to Game" the player drops into the live world. Mouse-look + WSAD
  free-fly, left-click breaks, right-click places stone, F2 saves a PNG
  screenshot, T or `/` opens the chat bar, F3 toggles the debug panel.
  Async load/save runs on a worker thread; meshing runs on a second worker.
  The world is saved to disk on exit. **All seven groups of the migration
  plan (`[A]`–`[F]`) have shipped.**

## Layout (current)

```
rs/src/
├── lib.rs                                 # crate root; declares modules
├── main.rs                                # tracing init + App::run()
├── setup.rs                               # tracing init helpers
│
├── app.rs                                 # winit ApplicationHandler;
│                                          #   30 Hz fixed-step accumulator [F1],
│                                          #   event → InputState translation,
│                                          #   per-frame egui + world render,
│                                          #   save_to_disk on exit
│
├── blocks.rs                              # Id, State, Light, BlockData,
│                                          #   TextureIndex, BlockInfo,
│                                          #   BlockRegistry, BaseBlocks
├── config.rs                              # TOML options
├── i18n.rs                                # one-TOML-per-language tables
├── input.rs                               # InputState + bitsets (no winit)
├── items.rs                               # ItemStack
├── particles.rs                           # ParticleSystem (sim, not GPU)
│
├── math/
│   ├── mod.rs                             # cgmath re-exports + aliases
│   ├── aabb.rs                            # generic Aabb3<S: BaseFloat>
│   ├── euler.rs                           # generic Euler<S: BaseFloat>
│   └── frustum.rs                         # generic Frustum<S: BaseFloat>
│
├── chunks/
│   ├── mod.rs                             # Chunk struct + storage methods
│   └── generate.rs                        # init_generate (terrain layering)
│
├── commands/
│   ├── mod.rs                             # Command, CommandRegistry
│   └── base.rs                            # register_base_commands (12 cmds)
│
├── worldgen.rs                            # Generator + HeightMap
│
├── game/                                  # Game orchestrator ([F1/F2/F3/F6])
│   ├── mod.rs                             # Game struct + tick / pump_meshing /
│   │                                      #   break / place / chat dispatch
│   ├── camera.rs                          # free-fly Y-up Camera + mouse-look
│   └── raycast.rs                         # Amanatides-Woo voxel DDA [F2]
│
├── gfx/                                   # the renderer ([C] + [D] + [F4/F6])
│   ├── mod.rs                             # module declarations + re-exports
│   ├── context.rs                         # Gfx (instance/adapter/device/
│   │                                      #   queue/surface) [C1]; surface
│   │                                      #   usage now includes COPY_SRC
│   │                                      #   for screenshot capture [F4]
│   ├── basic_pipeline.rs                  # tiny scaffold pipeline [C2]
│   ├── basic.wgsl                         # inline-WGSL colored triangle
│   ├── atlases.rs                         # block + UI texture atlases [C3]
│   ├── uniforms.rs                        # FrameUniforms / ModelUniforms /
│   │                                      #   FilterUniforms + UniformBuffer<T>
│   │                                      #   [C4]
│   ├── text.rs                            # glyphon TextRenderer [C5]
│   ├── egui_renderer.rs                   # EguiRenderer (egui 0.34) [E1]
│   ├── depth.rs                           # DepthTarget [D2]
│   ├── mesh.rs                            # CPU per-face culled meshing [D1]
│   ├── chunk_render.rs                    # ChunkMesh + ChunkPipeline [D2]
│   ├── chunk.wgsl                         # forward chunk shader + fog [D2/D4]
│   ├── particle_render.rs                 # ParticleVertex / Mesh /
│   │                                      #   Pipeline [D3]
│   ├── particle.wgsl                      # billboard particle shader
│   ├── mesh_pipeline.rs                   # off-thread mesh worker [F6]
│   └── screenshot.rs                      # surface readback → PNG [F4]
│
├── ui/                                    # immediate-mode UI ([E])
│   ├── mod.rs                             # module declarations + re-exports
│   ├── screen.rs                          # Screen trait + ScreenStack [E2]
│   ├── hud.rs                             # crosshair, debug panel, chat,
│   │                                      #   chat history, selection box [E4]
│   ├── inventory.rs                       # 4×10 item slot grid [E5]
│   └── screens/
│       ├── mod.rs                         # re-exports all screens
│       ├── title.rs                       # main menu [E3]
│       ├── world_select.rs                # world list (placeholder) [E3]
│       ├── create_world.rs                # name + seed form [E3]
│       ├── options.rs                     # FOV, render distance, VSync [E3]
│       └── game.rs                        # HUD + pause + inventory [E3]
│
└── worlds/                                # mirrors C++ src/worlds/
    ├── mod.rs                             # re-exports World + Player
    ├── world/
    │   ├── mod.rs                         # World struct + impl
    │   ├── grid.rs                        # ChunkGrid + ChunkKey
    │   ├── store.rs                       # TilesStore (sled)
    │   ├── error.rs                       # WorldError (thiserror)
    │   ├── pipeline.rs                    # async chunk load/save worker [F5]
    │   ├── tests.rs                       # World tests (cfg(test))
    │   └── test_support.rs                # shared TEST_LOCK + ScratchDir
    └── player/
        ├── mod.rs                         # Player + physics + tests
        └── save.rs                        # save_to / load_from + PlayerError

rs/tests/
└── smoke.rs                               # [F7] end-to-end smoke tests
```

Total: ~13 K LoC of Rust + 235 lines of WGSL across `rs/`.

## Migration plan tasks shipped

The plan splits work into seven groups (`[A]` through `[F]`). All seven
have shipped.

### `[A]` foundations — shipped (`e2c5a56` on `main`)

| Sub-task | Module | Notes |
|----------|--------|-------|
| A1 skeleton | `lib.rs`, `main.rs`, `setup.rs`, `Cargo.toml`, `assets/` | Single-crate, tracing init, GPL→CC0. |
| A2 math | `math::{aabb, euler, frustum, …}` | `cgmath` (chosen over `glam` for true scalar generics). `Aabb3<S>`, `Euler<S>`, `Frustum<S>`. |
| A3 config | `config::Config` | TOML at `configs/options.toml`. Atomic save. 18 fields. |
| A4 i18n | `i18n::I18n` | One TOML per language at `assets/lang/<code>.toml`. `get` returns `""` on miss. |
| A5 input | `input::InputState` | Pure data, no winit dep. `Key` / `MouseButton` enums + bitsets. `begin_frame()` clears per-frame transients. |
| A6 blocks | `blocks::*` | `Pod` newtypes; `TextureIndex` constants folded into `BlockInfo::faces`. `register_base_blocks` populates 19 base blocks. |
| A7 items | `items::ItemStack` | `#[repr(C)]` + `Pod` (with explicit `_pad`). `merge_into` helper. |

### `[B]` world model — shipped (`5b917d1` on `main`)

| Sub-task | Module | Notes |
|----------|--------|-------|
| B1 worldgen | `worldgen::{Generator, HeightMap}` | Direct port of the C++ noise math; the C++ `noise_2d` doesn't actually consume the seed. Flagged TODO. |
| B2 chunks | `chunks::{Chunk, ChunkError}` | Lazy `Box<[BlockData; 4096]>`. Save format: `NEWC` magic + version + flags + cells. |
| B3 player | `worlds::player::{Player, GameMode, PlayerError}` | `validate_block_placement` is a pure predicate. `save_to`/`load_from` is bincode v2 with `NEWP` magic + version. |
| B4 world | `worlds::world::{World, ChunkGrid, ChunkKey, TilesStore, BlockView, WorldError}` | Slab arena + `by_coord` map + sliding `ChunkGrid` (per plan §2.2/§4.6). `TilesStore` is sled-backed. |
| B5 commands | `commands::{Command, CommandRegistry, register_base_commands}` | All 12 C++ slash-commands ported. Deterministic `try_auto_complete`. |

### `[C]` graphics core — shipped (`7903b9b` on `main`)

| Sub-task | Module | Notes |
|----------|--------|-------|
| C1 winit + wgpu bring-up | `gfx::context::Gfx`, `app::App` | Window + surface (`Bgra8UnormSrgb` preferred, `Fifo` vsync) + clear color. |
| C2 basic pipeline | `gfx::basic_pipeline::BasicPipeline` + `basic.wgsl` | Inline-WGSL fullscreen-ish colored triangle. |
| C3 atlases | `gfx::atlases::{Atlases, AtlasArray, Atlas2d}` | 30-layer block diffuse + normal D2Array, single-2D block noise, 10 UI textures. `Nearest` sampler, `ClampToEdge`. |
| C4 uniforms | `gfx::uniforms::{FrameUniforms, ModelUniforms, FilterUniforms, UniformBuffer<T>}` | `#[repr(C)]` Pod structs; size-mod-16 compile-time asserts. |
| C5 glyphon text | `gfx::text::TextRenderer` | glyphon 0.11 wrapper. Bundles `unicode.ttf` via `include_bytes!`. |

### `[D]` world rendering — shipped (`4c060da` on `main`)

| Sub-task | Module | Notes |
|----------|--------|-------|
| D1 mesh | `gfx::mesh::{ChunkVertex, MeshInput, MeshOutput, mesh_chunk}` | Per-face culling against an 18³ padded buffer; 28-byte vertex. Greedy merge intentionally skipped. |
| D2 chunk render | `gfx::chunk_render::{ChunkMesh, ChunkPipeline}` + `gfx::depth::DepthTarget` + `chunk.wgsl` | Two pipelines (opaque + translucent) sharing layout + bind groups; Depth32Float; `coord * CHUNK_SIZE` baked into vertex positions on upload. |
| D3 particles | `crate::particles::ParticleSystem` (sim) + `gfx::particle_render::*` (GPU) + `particle.wgsl` | C++ gravity (`-0.03/tick`), drag (`0.6/tick`), AABB collision via `BlockView`. 32-byte billboard vertex. |
| D4 final pass | `gfx::chunk.wgsl` (header) | Distance fog (24..96 m linear band) and ambient sky tint folded into the chunk shader rather than a separate composition pass. |

### `[E]` UI — shipped (`912f00f` on `main`, squashed)

| Sub-task | Module | Notes |
|----------|--------|-------|
| E1 egui bring-up | `gfx::egui_renderer::EguiRenderer` | egui 0.34 + egui-winit + egui-wgpu; wgpu 29.0.1 compat. `forget_render_pass_lifetime` bridges the `RenderPass<'static>` requirement. |
| E2 screen framework | `ui::screen::{Screen, ScreenStack, Transition}` | `Screen` trait takes `&egui::Context`. `ScreenStack` push/pop/tick. `Transition` has `Push`/`Pop`/`Exit`. |
| E3 menu screens | `ui::screens::{Title,WorldSelect,CreateWorld,Options,Game}` | All five screens implemented as `Screen` trait impls. Title shows "Back to Game" / "Options" / "Quit". Create world has name + seed form. Options sets FOV (70–120), render distance (3–15), VSync, font scale, language. |
| E4 HUD overlay | `ui::hud::Hud` | Crosshair, debug panel (F3 toggle), chat bar (T or `/` toggle), chat history with 5 s decay, selection-box overlay. |
| E5 inventory | `ui::inventory::Inventory` | 4×10 slot grid in a centered `Window`. Each slot is a 32×32 px grey square with placeholder label + count. |

`GameScreen` composes `Hud` + `Inventory` and is ticked directly by
`App::frame` when the screen stack is empty; menu screens overlay via the
stack. The squashed `[E]` commit also includes the post-[E] fixes
(`fix(ui): pass egui TexturesDelta to wgpu renderer`,
`fix(ui): Pop on last screen returns to game, not exit`,
`fix(gfx): set minimum window size to prevent surface destruction`).

Cargo deps added at `[E]`: `egui = "0.34"`, `egui-winit = "0.34"`,
`egui-wgpu = "0.34"`.

### `[MVP]` minimum viable game — shipped (`039fe80` on `main`)

`crate::game::Game` owned the World + free-fly Camera + chunk meshes +
pipelines. Highlights at the time:

* Render distance 3 → 343 chunks loaded synchronously at startup; mesh +
  upload all of them; render every frame.
* World anchored at `y = 128`. Camera spawns at `(0, 160, 32)`.
* WSAD + Space/Shift movement at 18 m/s (5× with Ctrl). Mouse-look at
  0.0025 rad/px.

The MVP's synchronous chunk pipeline was replaced wholesale by `[F5]` /
`[F6]` async workers; `Game` now streams chunks in over the first few
frames rather than blocking on a fully-loaded world.

### Post-MVP texture fixes (`b5a1239`, `6922b65`)

Three correctness patches squashed into the MVP line:

* **Atlas layer order**: PNGs were authored to match the C++
  `load_png_image` Y-flip-on-decode, with the LAST atlas entry at the
  top of the file. The Rust `image` crate loads top-down, so the loader
  reads block `layers - 1 - texture_layer` for each slice. Without this,
  every block sampled an off-by-19 atlas layer.
* **Alpha test**: chunk shader changed `discard if a < 0.5` to `<= 0.0`
  so water (semi-transparent) survives.
* **World center / camera**: anchored chunk grid at world `y = 128` so
  the loaded chunks cover the generator surface. Camera far plane bumped
  256 → 1024.
* **Side-face V flip**: chunk vertex UVs use `[(0,1), (1,1), (1,0),
  (0,0)]` instead of `[(0,0), (1,0), (1,1), (0,1)]` to compensate for
  wgpu's `t = 0` being at the top of the texture (vs OpenGL's bottom).

### `[F]` orchestration — shipped (`6696045` on `main`, squashed)

| Sub-task | Module | Notes |
|----------|--------|-------|
| F1 fixed-step loop | `app::App` | 30 Hz accumulator (`TICK_DT = 1/30`, `MAX_TICKS_PER_FRAME = 5`). `Game::tick` runs once per slice; mouse motion is consumed only on the first tick of a frame. |
| F2 raycast + break/place | `game::raycast` + `game::Game::{try_break, try_place}` | Amanatides-Woo voxel DDA over `&impl BlockView`; left-click breaks (spawning ~10 LCG-jittered debris particles textured from `BlockInfo::face(0)`), right-click places stone on the entry-face normal. Camera-AABB rejection prevents trapping. |
| F3 chat + commands | `game::Game::submit_chat_line` + `ui::hud::Hud` | Enter submits; `/`-prefix dispatches via `CommandRegistry::execute_on`; plain text echoes into history (8-line window, 5 s decay). Tab autocompletes against the registry. Command-driven world mutations are picked up by snapshotting `Chunk::modified()` before/after dispatch. |
| F4 screenshot | `gfx::screenshot::Screenshot` | Surface usage now `RENDER_ATTACHMENT \| COPY_SRC`. F2 key triggers `copy_texture_to_buffer` + a worker thread that polls + maps + encodes a PNG. Output: `screenshots/screenshot_<unix seconds>.png`. |
| F5 async load/save | `worlds::world::pipeline::ChunkPipeline` + `World::{tick_chunk_loading_async, poll_load_results, request_save}` | Worker thread carries its own `HeightMap` + clones of `Arc<BlockRegistry>` and `Arc<sled::Db>`. `crossbeam-channel` for messages. `World::save_to_disk` runs synchronously on `App::exiting` so saves flush before the worker shuts down. |
| F6 async meshing | `gfx::mesh_pipeline::MeshPipeline` + `Game::pump_meshing` | Mesh worker runs `mesh_chunk` over owned `MeshInput` snapshots. Up to `MAX_MESH_DISPATCHES_PER_FRAME = 8` dirty coords are submitted each frame and the resulting `MeshOutput`s are uploaded as they arrive. Outputs are re-resolved by coord through `World::chunk_by_coord` per migration plan §2.5 — never via a stale `ChunkKey`. `chunk_meshes: HashMap<Vec3i, ChunkMesh>` lets unload drop entries by coord. |
| F7 smoke test | `tests/smoke.rs` | Two integration tests covering the launch-flow contract: open world → load chunks via async pipeline → set_block → save_to_disk → drop → reopen → verify; plus a registry/dispatch test (`/setblock`, `/help`). |

Launch flow per [F7] starts at the title screen on top of an
already-loaded world (`ui::initial_screen_stack` on `App::resumed`).
Clicking "Back to Game" pops the title and the user is in the game.

Cargo deps added at `[F]`: `crossbeam-channel = "0.5"`.

### Post-[F] cleanup (folded into `6696045`)

* `world/mod.rs` shrunk from 1100 to 824 LoC by moving tests into
  `world/tests.rs` and the shared `TEST_LOCK` / `ScratchDir` into
  `world/test_support.rs`. Consolidating the two duplicate `TEST_LOCK`
  statics fixed a cross-module flake where `world::tests` and
  `world::store::tests` could each chdir while the other was reading
  cwd.
* `game/mod.rs` shrunk from 822 to 696 LoC by extracting `Camera` (and
  the `OPENGL_TO_WGPU` constant) into `game::camera`.

## Deviations from the migration plan

* **Worldgen seed currently unused.** The C++ `noise_2d` doesn't actually
  mix the per-world `_seed` into its hash; the Rust port is a faithful
  1:1 copy and inherits the bug. Fix is a worldgen-internal change (use a
  `wrapping_mul`-friendly Wang-style mix); flagged with a `TODO`.
* **`World` cwd-relative.** `TilesStore::open` opens
  `worlds/<name>/chunks.db` relative to cwd; `Game::new` calls
  `ensure_world_root()` which `chdir`s into a temp directory before
  constructing the `World`. Future refactor: thread an explicit base path
  through `TilesStore::open_at`.
* **Commands tests are dispatch-only.** B5 stub-world tests were dropped
  during the merge with the real `World`. Per-command behaviour is now
  exercised end-to-end via `tests/smoke.rs` (`/setblock`, `/help`) plus
  internal world tests; per-command unit tests can be re-added when
  `World::new_in_memory` exists.
* **`ChunkSlot` collocation deferred.** Plan §2.2 says `Slab<ChunkSlot {
  chunk, render }>`. `World` ships plain `Slab<Chunk>`; the GPU-side
  `ChunkMesh` lives in a separate `HashMap<Vec3i, ChunkMesh>` on `Game`.
  Re-resolving by coord per §2.5 makes this safe; the joining is purely
  a layout change.
* **Smooth lighting / per-vertex AO not ported.** The C++ chunk mesher
  computes a 4-corner average of neighbor light per face vertex; the
  Rust port emits a single `face_id` and the shader does a flat lambert
  against `frame.sun_dir`. Restoring smooth lighting is a follow-up that
  needs an extra `color: u8` (or similar) per `ChunkVertex`.
* **No greedy face merging.** Every visible block face emits its own
  6-vertex quad. The C++ greedy merge is a future optimization.
* **Per-chunk model uniform skipped.** `ChunkMesh::upload` bakes the
  chunk world origin (`coord * CHUNK_SIZE`) into every vertex's
  `position` on the CPU, so the chunk shader needs only `view_proj`.
  When sub-block animation lands, this becomes a small group(2) uniform
  with dynamic offset.
* **No deferred renderer.** `[D4]` was simplified to "distance fog +
  ambient sky inside the chunk shader" rather than a full G-buffer +
  composition pass. Reversed-Z, shadow maps, SSR, and volumetric clouds
  are all skipped.
* **Save format break.** No backward compatibility with C++ saves.
  Player and chunk files are tagged with `u32` magic + `u32` version
  from day one.
* **egui 0.34 chosen over 0.31 for wgpu 29 compat.** egui-wgpu 0.34.1
  is the first release that shares wgpu 29.0.1 with the project. The
  `unsafe` lifetime-forget transmute in `egui_renderer.rs` bridges the
  `RenderPass<'static>` requirement in egui-wgpu — the pass does not
  actually borrow anything with its lifetime parameter (see wgpu#1671).
* **`World::new` still synchronous.** Async chunk loading sits BEHIND
  `World::new` — the constructor returns immediately, then the
  pipeline streams chunks in once the caller starts pumping
  `tick_chunk_loading_async` + `poll_load_results`. The C++ `World`
  constructor blocked on initial chunk gen; the Rust port doesn't.
* **No mid-game chunk unload pressure.** `Game::tick` doesn't currently
  call `set_center` or trigger unloads as the player walks far away —
  the renderer keeps everything ever loaded. The async pipeline supports
  this; wiring is a small follow-up.

## Conventions established along the way

* **`mod.rs` everywhere.** Even single-file modules get
  `<name>/mod.rs`-on-its-own when they have submodules; flat `<name>.rs`
  is reserved for leaves with no submodules.
* **Reflect C++ packaging.** `rs/src/worlds/{world,player}/…` mirrors
  `src/worlds/{worlds,player,…}.ixx`. Top-level Rust modules correspond
  to top-level C++ `.ixx` files.
* **No backward compatibility with C++ saves or option files.** All
  formats are tagged with magic+version from the start.
* **Each persistent format gets a 4-byte ASCII magic and a `u32`
  version.** `NEWC` for chunks, `NEWP` for player.
* **`unsafe_code = "deny"`** at the crate level (with one localized
  exception in `gfx::egui_renderer` for the `RenderPass<'static>`
  lifetime bridge).
* **`Pod` types** (`Id`, `State`, `Light`, `BlockData`, `TextureIndex`,
  `ItemStack`, `ChunkVertex`, uniform structs) are `#[repr(C)]` and
  avoid implicit padding (explicit `_pad` where needed).
* **`BaseBlocks` passed explicitly** to every accessor that needs the
  `air` id. The C++ globals (`base_blocks()`, `block_info_registry()`)
  are deliberately not ported.
* **Test harness pattern.** `world/test_support.rs` provides one
  process-global `TEST_LOCK` + `ScratchDir` shared across every
  `world::*` test module — necessary because cwd changes have
  process-wide effect and sled rejects concurrent opens of the same DB.
* **Cross-thread chunk references go by `IVec3` coord, not `ChunkKey`.**
  Both [F5] load results and [F6] mesh outputs re-resolve through
  `World::chunk_by_coord` so a slot recycled mid-flight is never
  aliased (migration plan §2.5).
* **wgpu top-down texture convention is honored end-to-end.** The atlas
  uploader reverses the layer order (per the C++ Y-flip-on-load) and the
  chunk mesh flips the V coordinate, so the visual top of every
  per-block art square ends up at the visual top of every face when
  rendered.

## Open work (future polish)

The migration plan groups are all complete. Smaller follow-ups remain:

* **Worldgen seed wiring** (`worldgen.rs`) — the C++ `noise_2d` ignores
  `_seed`; rework with a Wang-style mix so the seed actually affects
  output.
* **Smooth lighting / per-vertex AO** (`gfx::mesh` + `chunk.wgsl`) —
  add a `color: u8` attribute and the 4-corner light average from the
  C++ mesher.
* **Greedy face merge** (`gfx::mesh::mesh_chunk`) — collapse coplanar
  same-texture quads.
* **Reversed-Z depth** (`chunk_render.rs` + `depth.rs`) — switch to
  `Greater` compare and a 0.0 clear once depth precision matters at the
  far plane.
* **`TilesStore::open_at(&Path)`** — thread an explicit base path through
  so tests + the MVP no longer have to `chdir`.
* **`set_center` + chunk unload as the player walks** — the world's
  unload path exists and works in tests; `Game::tick` just doesn't call
  it yet.
* **Block-update queue draining** — `World::process_block_updates`
  exists but `Game::tick` doesn't drive it; would let TNT/lava/etc.
  actually propagate.
* **Smooth-lighting / per-block-face art improvements** beyond what the
  shader does today (lambert + fog + ambient-sky tint).

## Repository state

* `main` is at `6696045`, with the linear feature line:
  ```
  6696045 [F] orchestration: fixed-step loop + raycast + chat + screenshots + async pipeline
  912f00f [E] UI layer: egui integration + menu screens + HUD + inventory
  217a01b docs: refresh progress log through [D] + MVP + post-MVP fixes
  6922b65 fix(gfx): flip V on chunk vertex UVs for wgpu top-down convention
  b5a1239 fix(gfx): atlas layer order + alpha test + camera spawn
  039fe80 feat: minimum viable game (static world + free-fly camera)
  4c060da [D] world rendering: mesh + chunk pipeline + particles + final pass
  7903b9b [C] graphics core: winit + wgpu + atlases + uniforms + text
  5f74c24 docs: add progress log
  5b917d1 [B] world model: worldgen + chunks + worlds (world/player) + commands
  e2c5a56 feat(foundations): cargo skeleton + math/config/i18n/input/blocks/items
  7d7e444 docs: add Rust migration plan
  ```
* Worktrees from agent runs live under `.claude/worktrees/`. They are
  locked by the harness while sessions are open and are reaped on
  session end.

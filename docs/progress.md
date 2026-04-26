# Rust port — progress log

This document tracks the state of the Rust port at `rs/`. The migration plan
itself lives in [`rust_migration.md`](rust_migration.md); this file records
which sections of the plan have shipped, what their shape ended up being, and
what's still open.

## Current state

* Crate layout: single `rs/` Cargo crate, edition 2024, Rust 1.95+.
* License: `CC0-1.0`.
* Lints: `cargo clippy --all-targets -- -D warnings` is clean.
* Tests: 137 lib unit tests + 2 smoke tests + 13 world integration tests,
  all passing.
* The binary opens a 1280×720 window into a title menu, descends through
  Singleplayer → world list → "Enter" to load a world (or "Create New World"
  to make one). On load, the world streams chunks in asynchronously around
  the player. Mouse-look turns the player; WSAD walks (double-tap W to
  sprint); Space jumps; Shift crouches/dives; gravity + hitbox collision +
  fall damage all run at 30 Hz. Left-click breaks, right-click places from
  the held hotbar slot. Z/X and the mouse wheel cycle the hotbar. E opens
  the inventory grid (with mouse-driven pickup / place / split). F2 saves
  a PNG screenshot, T or `/` opens the chat bar, F3 toggles debug overlay,
  F1 swaps creative ↔ survival, F4 toggles cross-wall in creative. Pause
  → "Save & Quit to Title" persists the world and returns to the menu.
  **All seven groups of the migration plan (`[A]`–`[F]`) have shipped.**

## Layout (current)

The Rust tree mirrors `src/` in the C++ build wherever the mapping is
direct, so a feature can be diffed across builds.

```
rs/src/
├── lib.rs                                 # crate root; module table
├── main.rs                                # tracing init + App::run()
├── setup.rs                               # tracing init helpers
│
├── app.rs                                 # winit ApplicationHandler:
│                                          #   30 Hz fixed-step accumulator [F1],
│                                          #   event → InputState translation,
│                                          #   per-frame egui + world render,
│                                          #   WorldAction queue draining,
│                                          #   save_to_disk on exit
│
├── blocks.rs                              # ↔ blocks.ixx
├── config.rs                              # ↔ globals.ixx (options part)
├── globalization.rs                       # ↔ globalization.ixx
├── height_maps.rs                         # ↔ height_maps.ixx
├── input.rs                               # InputState + bitsets (no winit)
├── items.rs                               # ↔ items.ixx
├── particles.rs                           # ↔ particles.ixx
├── terrain_generation.rs                  # ↔ terrain_generation.ixx
├── text_rendering.rs                      # ↔ text_rendering.ixx
├── textures.rs                            # ↔ textures.ixx
│
├── chunks/                                # ↔ chunks.ixx
│   ├── mod.rs                             # Chunk + storage
│   └── generate.rs                        # init_generate (terrain layering)
│
├── commands/                              # ↔ commands.ixx
│   ├── mod.rs
│   └── base.rs                            # 12 base commands
│
├── game/                                  # in-process game orchestrator
│   ├── mod.rs                             # Game::tick_render / tick_sim /
│   │                                      #   pump_meshing / break / place /
│   │                                      #   chat dispatch
│   ├── camera.rs                          # passive view; mirrors player
│   └── raycast.rs                         # Amanatides–Woo voxel DDA
│
├── math/                                  # ↔ math/*.ixx
│   ├── mod.rs                             # cgmath re-exports + aliases
│   ├── aabb.rs                            # generic Aabb3<S>
│   ├── euler.rs                           # generic Euler<S>
│   └── frustum.rs                         # generic Frustum<S>
│
├── menus/                                 # ↔ menus/*.cpp
│   ├── mod.rs
│   ├── main_menu.rs                       # ↔ main_menu.cpp
│   ├── world_menu.rs                      # ↔ world_menu.cpp
│   ├── create_world_menu.rs               # ↔ create_world_menu.cpp
│   ├── options_menu.rs                    # ↔ options_menu.cpp
│   └── game_menu.rs                       # ↔ game_menu.cpp (composes HUD +
│                                          #   inventory + pause; the C++
│                                          #   draws HUD/inventory inline)
│
├── render/                                # ↔ render/*.ixx (wgpu replacement
│   │                                      #   for the GL wrappers)
│   ├── mod.rs
│   ├── basic_pipeline.rs                  # scaffold triangle pipeline
│   ├── context.rs                         # Gfx (instance/adapter/device/
│   │                                      #   queue/surface) [C1]
│   ├── depth.rs                           # DepthTarget [D2]
│   ├── egui_renderer.rs                   # egui ↔ wgpu bridge [E1]
│   ├── mesh.rs                            # CPU per-face culled meshing [D1]
│   ├── mesh_pipeline.rs                   # off-thread mesh worker [F6]
│   ├── particle_render.rs                 # billboard particle pipeline [D3]
│   ├── screenshot.rs                      # surface readback → PNG [F4]
│   └── uniforms.rs                        # FrameUniforms / ModelUniforms /
│                                          #   FilterUniforms + UniformBuffer<T>
│
├── ui/                                    # Rust-specific UI infra
│   ├── mod.rs
│   ├── action.rs                          # WorldActionQueue (cross-screen
│   │                                      #   "Enter / Leave / Delete world")
│   ├── hud.rs                             # crosshair, debug, chat, selection
│   ├── inventory.rs                       # 4×10 grid + always-on hotbar +
│   │                                      #   mouse pickup / place / split
│   └── screen.rs                          # Screen trait + ScreenStack
│
└── worlds/                                # ↔ worlds/{worlds,player,…}
    ├── mod.rs
    ├── chunk_rendering.rs                 # ↔ worlds/chunk_rendering.cpp
    │                                      #   (ChunkMesh + ChunkPipeline)
    ├── player/                            # ↔ player.ixx + player_impl.cpp
    │   ├── mod.rs                         # Player + physics
    │   └── save.rs                        # NEWP save format
    └── world/                             # ↔ worlds.ixx
        ├── mod.rs                         # World struct + impl
        ├── error.rs                       # WorldError (thiserror)
        ├── pipeline.rs                    # async chunk load/save worker [F5]
        └── store.rs                       # TilesStore (sled)

rs/shaders/                                # WGSL sources, included via
├── basic.wgsl                             #   include_str! at compile time
├── chunk.wgsl                             #   (kept outside src/ so they're
└── particle.wgsl                          #   easy to edit + diff)

rs/tests/
├── common/mod.rs                          # ScratchDir helper
├── smoke.rs                               # end-to-end launch smoke
└── world.rs                               # World integration suite
```

Total: ~14.5 K LoC of Rust + 235 lines of WGSL across `rs/`.

## Migration plan tasks shipped

The plan splits work into seven groups (`[A]` through `[F]`). All seven
have shipped.

### `[A]` foundations — shipped (`e2c5a56` on `main`)

| Sub-task | Module | Notes |
|----------|--------|-------|
| A1 skeleton | `lib.rs`, `main.rs`, `setup.rs`, `Cargo.toml`, `assets/` | Single-crate, tracing init, GPL→CC0. |
| A2 math | `math::{aabb, euler, frustum, …}` | `cgmath` (chosen over `glam` for true scalar generics). `Aabb3<S>`, `Euler<S>`, `Frustum<S>`. |
| A3 config | `config::Config` | TOML at `configs/options.toml`. Atomic save. 18 fields. |
| A4 i18n | `globalization::I18n` | One TOML per language at `assets/lang/<code>.toml`. `get` returns `""` on miss. |
| A5 input | `input::InputState` | Pure data, no winit dep. `Key` / `MouseButton` enums + bitsets. `begin_frame()` clears per-frame transients. |
| A6 blocks | `blocks::*` | `Pod` newtypes; `TextureIndex` constants folded into `BlockInfo::faces`. `register_base_blocks` populates 19 base blocks. |
| A7 items | `items::ItemStack` | `#[repr(C)]` + `Pod` (with explicit `_pad`). `merge_into` helper. |

### `[B]` world model — shipped (`5b917d1`)

Scope as committed; the `World` itself was further simplified later — see
[Post-`[F]` simplifications](#post-f-simplifications).

| Sub-task | Module | Notes |
|----------|--------|-------|
| B1 worldgen | `terrain_generation`, `height_maps` | Direct port of the C++ noise math. Generator + sliding 2D height cache as separate sibling files. The C++ `noise_2d` doesn't actually consume the seed — flagged `TODO` in `terrain_generation.rs`. |
| B2 chunks | `chunks::{Chunk, ChunkError}` | Lazy `Box<[BlockData; 4096]>`. Save format: `NEWC` magic + version + flags + cells. |
| B3 player | `worlds::player` | `validate_block_placement` is a pure predicate. `save_to`/`load_from` is bincode v2 with `NEWP` magic + version. |
| B4 world | `worlds::world` | See [Post-`[F]` simplifications](#post-f-simplifications) — the slab + ChunkGrid layered storage was collapsed into a plain `HashMap<Vec3i, Chunk>` once the rest of the system stabilized. `TilesStore` is sled-backed. |
| B5 commands | `commands::*` | All 12 C++ slash-commands ported. Deterministic `try_auto_complete`. |

### `[C]` graphics core — shipped (`7903b9b`)

| Sub-task | Module | Notes |
|----------|--------|-------|
| C1 winit + wgpu bring-up | `render::context::Gfx`, `app::App` | Window + surface (`Bgra8UnormSrgb` preferred, `Fifo` vsync) + clear color. |
| C2 basic pipeline | `render::basic_pipeline` + `shaders/basic.wgsl` | Inline-WGSL fullscreen-ish colored triangle. |
| C3 atlases | `textures::{Atlases, AtlasArray, Atlas2d}` | 30-layer block diffuse + normal D2Array, single-2D block noise, 10 UI textures. `Nearest` sampler, `ClampToEdge`. |
| C4 uniforms | `render::uniforms` | `#[repr(C)]` Pod structs; size-mod-16 compile-time asserts. |
| C5 glyphon text | `text_rendering::TextRenderer` | glyphon 0.11 wrapper. Bundles `unicode.ttf` via `include_bytes!`. |

### `[D]` world rendering — shipped (`4c060da`)

| Sub-task | Module | Notes |
|----------|--------|-------|
| D1 mesh | `render::mesh` | Per-face culling against an 18³ padded buffer; 28-byte vertex. Greedy merge intentionally skipped. |
| D2 chunk render | `worlds::chunk_rendering` + `render::depth` + `shaders/chunk.wgsl` | Two pipelines (opaque + translucent) sharing layout + bind groups; Depth32Float; `coord * CHUNK_SIZE` baked into vertex positions on upload. |
| D3 particles | `crate::particles::ParticleSystem` (sim) + `render::particle_render::*` (GPU) + `shaders/particle.wgsl` | C++ gravity (`-0.03/tick`), drag (`0.6/tick`), AABB collision via `BlockView`. 32-byte billboard vertex. |
| D4 final pass | `shaders/chunk.wgsl` (header) | Distance fog (band scales with render distance) + ambient sky tint folded into the chunk shader rather than a separate composition pass. |

### `[E]` UI — shipped (`912f00f`, squashed)

| Sub-task | Module | Notes |
|----------|--------|-------|
| E1 egui bring-up | `render::egui_renderer::EguiRenderer` | egui 0.34 + egui-winit + egui-wgpu; wgpu 29.0.1 compat. `forget_render_pass_lifetime` bridges the `RenderPass<'static>` requirement. |
| E2 screen framework | `ui::screen::{Screen, ScreenStack, Transition}` | Push/pop/tick. `Pop` on the last screen returns to the game. |
| E3 menu screens | `menus::{main_menu,world_menu,create_world_menu,options_menu,game_menu}` | One file per screen; file names mirror C++ `menus/*_menu.cpp`. |
| E4 HUD overlay | `ui::hud::Hud` | Crosshair, debug panel (F3), chat bar (T or `/`), chat history with 5 s decay, selection-box overlay. |
| E5 inventory | `ui::inventory::Inventory` | 4×10 slot grid + always-on hotbar; mouse left-click pickup/place/swap, right-click split/place-one. |

Cargo deps added at `[E]`: `egui = "0.34"`, `egui-winit = "0.34"`,
`egui-wgpu = "0.34"`.

### `[F]` orchestration — shipped (`6696045`, squashed)

| Sub-task | Module | Notes |
|----------|--------|-------|
| F1 fixed-step loop | `app::App` | 30 Hz accumulator (`TICK_DT = 1/30`, `MAX_TICKS_PER_FRAME = 5`). `Game::tick_sim` runs once per slice; `Game::tick_render` runs every frame so mouse-look stays smooth at high FPS. |
| F2 raycast + break/place | `game::raycast` + `Game::{try_break, try_place}` | Amanatides-Woo voxel DDA; left-click breaks (spawning ~10 LCG-jittered debris particles textured from `BlockInfo::face(0)`); right-click places the held-hotbar block id on the entry-face normal, decrementing the held stack. Player-hitbox rejection prevents trapping. |
| F3 chat + commands | `Game::submit_chat_line` + `ui::hud::Hud` | Enter submits; `/`-prefix dispatches via `CommandRegistry::execute_on`. Tab autocompletes against the registry. Command-driven mutations are picked up by snapshotting `Chunk::modified()` over `World::non_empty_chunks()` before/after dispatch. |
| F4 screenshot | `render::screenshot::Screenshot` | Surface usage now `RENDER_ATTACHMENT \| COPY_SRC`. F2 key triggers `copy_texture_to_buffer` + a worker thread that polls + maps + encodes a PNG. Output: `screenshots/screenshot_<unix seconds>.png`. |
| F5 async load/save | `worlds::world::pipeline::ChunkPipeline` + `World::{tick_chunk_loading_async, poll_load_results, request_save}` | Worker thread carries its own `HeightMap` + clones of `Arc<BlockRegistry>` and `Arc<sled::Db>`. `crossbeam-channel` for messages. `World::save_to_disk` runs synchronously on `App::exiting`. |
| F6 async meshing | `render::mesh_pipeline::MeshPipeline` + `Game::pump_meshing` | Up to `MAX_MESH_DISPATCHES_PER_FRAME = 8` dirty coords are submitted each frame, sorted by squared chunk-distance to the player so the visible neighbourhood meshes first. Outputs are re-checked through `World::is_loaded` — never via a stale chunk reference. |
| F7 smoke test | `tests/smoke.rs` | Two integration tests covering the launch-flow contract: open world → load chunks via async pipeline → set_block → save_to_disk → drop → reopen → verify; plus a registry/dispatch test (`/setblock`, `/help`). |

Cargo deps added at `[F]`: `crossbeam-channel = "0.5"`.

## Post-`[F]` simplifications

The migration-plan groups all shipped, but the codebase has gone through
two rounds of structural cleanup since then.

### Player physics + world lifecycle (`6bbab06` on `main`)

The free-fly Camera was replaced with C++-style player physics. The `Game`
struct now drives the `worlds::Player` (which owns `update`, `on_jump`,
`on_crouch`, etc. unchanged from `[B3]`). WSAD adds horizontal velocity
in the player heading frame; Space/Shift map to `on_jump` / `on_crouch`;
double-tap W triggers sprinting via `Player::set_running`. The `Camera`
struct is now passive — every frame, `Game::tick_render` mirrors
`player.orientation()` into `camera.yaw/pitch` and `Game::write_frame_uniforms`
reads the interpolated eye position (`look_coord - velocity * (1 - α)`).

Same commit reshaped the app lifecycle: `Game` is `Option<Game>` in
`AppState`. The app boots into the title screen with no world loaded.
`menus/main_menu.rs` exposes a `Singleplayer` button that pushes
`menus/world_menu.rs`, which lists every directory under `<worlds_root>/`
(refreshed every frame so freshly-created worlds appear immediately).
Pressing "Enter" submits a `WorldAction::Enter { name, seed }` into the
`ui::action::WorldActionQueue`; the app drains the queue at the top of
each frame and constructs / tears down the `Game` accordingly. Pause →
"Save & Quit to Title" submits `WorldAction::LeaveToTitle`.

The inventory grew real mouse semantics: the always-on hotbar at the
bottom of the screen + the full 4×10 grid (E to open) both hand
mouse clicks to `ui::inventory`, which mutates the live `Player::inventory`.
Block break drops one of the broken id into the inventory; place pulls
from the held hotbar slot.

### Module layout mirrors C++ tree (last refactor)

Files were moved with `git mv` so history follows:

* `gfx/` → `render/` (matches C++ `src/render/`)
* `i18n.rs` → `globalization.rs` (matches C++ `globalization.ixx`)
* `worldgen.rs` → split into `terrain_generation.rs` + `height_maps.rs`
* `gfx/atlases.rs` → `textures.rs` (matches `textures.ixx`)
* `gfx/text.rs` → `text_rendering.rs` (matches `text_rendering.ixx`)
* `gfx/chunk_render.rs` → `worlds/chunk_rendering.rs`
* `ui/screens/<name>.rs` → `menus/<name>_menu.rs`
* WGSL sources moved to top-level `rs/shaders/` (out of `src/`)

`app.rs` was kept as-is; renaming to `neworld.rs` would create the awkward
path `neworld::neworld::App`, and the C++ god-file's logic is split across
`app.rs` + `game/` + `ui/hud.rs` + `ui/inventory.rs` anyway. `lib.rs`
carries a C++ ↔ Rust file-mapping table for the diff workflow.

### `World` storage simplification (this commit)

The `[B4]` design used a `slab::Slab<Chunk>` arena + `HashMap<Vec3i, ChunkKey>`
+ a sliding `ChunkGrid<Option<ChunkKey>>` for a fast hot-path lookup. With
the rest of the system stable, the layered storage stopped paying for
itself — every actual access is a single hash-map lookup at the call site,
and the cross-thread paths already re-check loadedness by coord. The
storage collapsed to:

```rust
pub struct World {
    chunks: HashMap<Vec3i, Chunk>,    // every loaded chunk, keyed by coord
    non_empty: HashSet<Vec3i>,        // subset whose Chunk::empty() == false
    …
}
```

`ChunkKey`, `ChunkGrid`, the slab arena, the `by_coord` map, and the
`slab` Cargo dep are all gone. The public API is coord-only: `chunk(c)`,
`is_loaded(c)`, `set_block(coord, id, queue_update)`, `block(coord)`.

The `non_empty` invariant — `non_empty.contains(c) ⇔ !chunks[c].empty()`
— is maintained by a single private helper `World::refresh_non_empty(c)`,
which every mutator (`set_block`, `update_block`, `poll_load_results`,
`load_chunk`) calls after touching a chunk through any code path that
might lazily allocate (`Chunk::block_mut`, `unpackage_from`,
`init_generate`). Removal goes through `unload_chunk` /
`unload_chunk_async`, which drop from both sets atomically.

The set powers the per-frame meshing / render / save loops:
`World::non_empty_chunks()` is `O(non-empty-count)` — pure-air sky chunks
above the terrain don't get scanned just to confirm they have nothing to
draw. Sorting load / unload / meshing candidates by squared distance to
the player is unchanged; the new structure just makes "skip empty chunks"
trivially cheap.

## Deviations from the migration plan

* **Worldgen seed currently unused.** The C++ `noise_2d` doesn't actually
  mix the per-world `_seed` into its hash; the Rust port is a faithful
  1:1 copy and inherits the bug. Fix is a worldgen-internal change (use a
  `wrapping_mul`-friendly Wang-style mix); flagged with a `TODO`.
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
* **`app.rs` not renamed to `neworld.rs`.** The closest C++ analog is
  `neworld.ixx` (the god-file with `main`, the game loop, HUD drawing,
  inventory drawing, …), but that logic is split across `app.rs` +
  `game/mod.rs` + `ui/hud.rs` + `ui/inventory.rs`. Forcing the rename
  would also create the awkward path `neworld::neworld::App` since the
  crate is named `neworld`.

## Conventions established along the way

* **Direct C++ ↔ Rust file mapping.** Where the file shape is similar,
  the Rust file is named after the C++ file (`globalization.rs`,
  `text_rendering.rs`, `terrain_generation.rs`, `menus/*_menu.rs`, …).
  `lib.rs` carries the mapping table for the diff workflow. Rust-only
  glue (winit context, wgpu pipelines, async workers, egui bridge) lives
  next to its closest C++ neighbour.
* **`mod.rs` for modules with submodules** (`game/mod.rs`, `chunks/mod.rs`,
  `worlds/world/mod.rs`, …); flat `<name>.rs` for leaf modules.
* **No backward compatibility with C++ saves or option files.** All
  formats are tagged with magic+version from the start.
* **Each persistent format gets a 4-byte ASCII magic and a `u32`
  version.** `NEWC` for chunks, `NEWP` for player.
* **`unsafe_code = "deny"`** at the crate level (with one localized
  exception in `render::egui_renderer` for the `RenderPass<'static>`
  lifetime bridge).
* **`Pod` types** (`Id`, `State`, `Light`, `BlockData`, `TextureIndex`,
  `ItemStack`, `ChunkVertex`, uniform structs) are `#[repr(C)]` and
  avoid implicit padding (explicit `_pad` where needed).
* **`BaseBlocks` passed explicitly** to every accessor that needs the
  `air` id. The C++ globals (`base_blocks()`, `block_info_registry()`)
  are deliberately not ported.
* **Coord-based chunk identity.** External callers refer to chunks by
  `Vec3i` chunk coord; the slab/key indirection is gone. The async
  pipelines (load/save in `worlds::world::pipeline`, mesh in
  `render::mesh_pipeline`) ship coords across threads and re-check
  `World::is_loaded(coord)` after the worker returns.
* **wgpu top-down texture convention is honored end-to-end.** The atlas
  uploader reverses the layer order (per the C++ Y-flip-on-load) and the
  chunk mesh flips the V coordinate, so the visual top of every
  per-block art square ends up at the visual top of every face when
  rendered.
* **WGSL sources live at `rs/shaders/*.wgsl`** and are pulled in via
  `include_str!("../../shaders/<name>.wgsl")` so they're easy to edit
  outside the Rust tree.

## Open work (future polish)

The migration plan groups are all complete. Smaller follow-ups remain:

* **Worldgen seed wiring** (`terrain_generation.rs`) — the C++ `noise_2d`
  ignores `_seed`; rework with a Wang-style mix so the seed actually
  affects output.
* **Smooth lighting / per-vertex AO** (`render::mesh` + `shaders/chunk.wgsl`)
  — add a `color: u8` attribute and the 4-corner light average from the
  C++ mesher.
* **Greedy face merge** (`render::mesh::mesh_chunk`) — collapse coplanar
  same-texture quads.
* **Reversed-Z depth** (`worlds::chunk_rendering` + `render::depth`) —
  switch to `Greater` compare and a 0.0 clear once depth precision
  matters at the far plane.
* **Block-update queue draining** — `World::process_block_updates`
  exists but `Game::tick_sim` doesn't drive it; would let TNT/lava/etc.
  actually propagate.
* **Mid-game render-distance resize** — currently `render_distance` is
  captured by `Game::new`; changing the option in the in-game settings
  takes effect on the next world load.
* **Block icon textures in inventory** — the inventory currently shows
  abbreviated block names instead of icons. Bridging the existing
  `texture_2d_array` into egui as per-layer `TextureId`s would let the
  hotbar / grid show real block art.

## Repository state

Linear feature line on `main`:

```
<head>   refactor(world): drop slab + grid; HashMap + non_empty invariant +
                          C++-mirroring module layout + .gitignore for runtime dirs
6bbab06  feat: player physics, world lifecycle, inventory mouse UI
e06764e  fix(gfx): fog scales with render distance + dirty chunks mesh closest-first
5869ff4  feat(config): live-wire options screen to camera/surface/egui/world
6217e80  test: move world tests to integration; add World::new_at + TilesStore::open_at
fd689ef  feat(game): immediate camera, dynamic chunk follow, frame-rate decoupled tick
48e2363  docs: refresh progress log through [E] squash + [F] shipped
6696045  [F] orchestration: fixed-step loop + raycast + chat + screenshots + async pipeline
912f00f  [E] UI layer: egui integration + menu screens + HUD + inventory
217a01b  docs: refresh progress log through [D] + MVP + post-MVP fixes
6922b65  fix(gfx): flip V on chunk vertex UVs for wgpu top-down convention
b5a1239  fix(gfx): atlas layer order + alpha test + camera spawn
039fe80  feat: minimum viable game (static world + free-fly camera)
4c060da  [D] world rendering: mesh + chunk pipeline + particles + final pass
7903b9b  [C] graphics core: winit + wgpu + atlases + uniforms + text
5f74c24  docs: add progress log
5b917d1  [B] world model: worldgen + chunks + worlds (world/player) + commands
e2c5a56  feat(foundations): cargo skeleton + math/config/i18n/input/blocks/items
7d7e444  docs: add Rust migration plan
```

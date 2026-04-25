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
* Tests: 137 unit tests passing single-threaded. No integration tests yet.
* Repository on-disk layout under `rs/src/` mirrors the C++ package
  structure and uses `mod.rs` for every multi-file module.
* The binary opens a 1280×720 window, generates a 7×7×7 chunk world
  (343 chunks, ~1.4 M cells) at startup, meshes every chunk on the CPU,
  uploads the resulting vertex buffers to wgpu, and renders the world each
  frame with a free-fly WSAD + mouse-look camera. An egui-driven HUD overlays debug info, crosshair, chat bar, and
  inventory. Menu screens (title, world select, create world, options) are
  reachable from the in-game pause menu. Only `[F]` (raycast / async
  pipeline / save persistence) is still skipped.

## Layout (after [E] UI layer)

```
rs/src/
├── lib.rs                                 # crate root; declares modules
├── main.rs                                # tracing init + App::run()
├── setup.rs                               # tracing init helpers
│
├── app.rs                                 # winit ApplicationHandler;
│                                          #   translates events → InputState,
│                                          #   drives Game::tick + render
├── game.rs                                # Game: world + camera + chunk
│                                          #   meshes + pipelines (MVP wiring)
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
├── gfx/                                   # the renderer ([C] + [D])
│   ├── mod.rs                             # module declarations + re-exports
│   ├── context.rs                         # Gfx (instance/adapter/device/
│   │                                      #   queue/surface) [C1]
│   ├── basic_pipeline.rs                  # tiny scaffold pipeline [C2]
│   ├── basic.wgsl                         # inline-WGSL colored triangle
│   ├── atlases.rs                         # block + UI texture atlases [C3]
│   ├── uniforms.rs                        # FrameUniforms / ModelUniforms /
│   │                                      #   FilterUniforms + UniformBuffer<T>
│   │                                      #   [C4]
│   ├── text.rs                            # glyphon TextRenderer [C5]
│   ├── egui_renderer.rs                   # EguiRenderer (egui 0.34) [E1]
│   ├── depth.rs                           # DepthTarget [D2]
│   ├── mesh.rs                            # CPU greedy meshing [D1]
│   ├── chunk_render.rs                    # ChunkMesh + ChunkPipeline [D2]
│   ├── chunk.wgsl                         # forward chunk shader + fog [D2/D4]
│   ├── particle_render.rs                 # ParticleVertex / Mesh /
│   │                                      #   Pipeline [D3]
│   └── particle.wgsl                      # billboard particle shader
│
├── ui/                                    # immediate-mode UI ([E])
│   ├── mod.rs                             # module declarations + re-exports
│   ├── screen.rs                          # Screen trait + ScreenStack [E2]
│   ├── hud.rs                             # crosshair, debug panel, chat [E4]
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
    │   ├── mod.rs                         # World struct + impl + tests
    │   ├── grid.rs                        # ChunkGrid + ChunkKey
    │   ├── store.rs                       # TilesStore (sled)
    │   └── error.rs                       # WorldError (thiserror)
    └── player/
        ├── mod.rs                         # Player + physics + tests
        └── save.rs                        # save_to / load_from + PlayerError
```

Total: ~11.7 K LoC of Rust + 235 lines of WGSL across `rs/src/`.

## Migration plan tasks shipped

The plan splits work into seven groups (`[A]` through `[F]`). Groups
`[A]`, `[B]`, `[C]`, `[D]`, and `[E]` are complete. Only `[F]`
(raycast / async pipeline / save persistence / chat / screenshots) remains
skipped — see `[MVP]` below.

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
| B2 chunks | `chunks::{Chunk, ChunkError}` | Lazy `Box<[BlockData; 4096]>`. `block`/`block_mut` take explicit `&BaseBlocks`. Save format: `NEWC` magic + version + flags + cells. |
| B3 player | `worlds::player::{Player, GameMode, PlayerError}` | `validate_block_placement` is a pure predicate; caller does the world write. `save_to`/`load_from` is bincode v2 with `NEWP` magic + version. |
| B4 world | `worlds::world::{World, ChunkGrid, ChunkKey, TilesStore, BlockView, WorldError}` | Slab arena + `by_coord` map + sliding `ChunkGrid` (per plan §2.2/§4.6). `TilesStore` is sled-backed. |
| B5 commands | `commands::{Command, CommandRegistry, register_base_commands}` | All 12 C++ slash-commands ported. `/time` is wired through. Deterministic `try_auto_complete`. |

### `[C]` graphics core — shipped (`7903b9b` on `main`)

| Sub-task | Module | Notes |
|----------|--------|-------|
| C1 winit + wgpu bring-up | `gfx::context::Gfx`, `app::App` | Window + surface (`Bgra8UnormSrgb` preferred, `Fifo` vsync) + clear color. winit 0.30 `ApplicationHandler`. wgpu 29.0.1, no `unsafe`. |
| C2 basic pipeline | `gfx::basic_pipeline::BasicPipeline` + `basic.wgsl` | Inline-WGSL fullscreen-ish colored triangle. The actual C++ shaders are deliberately not ported here. |
| C3 atlases | `gfx::atlases::{Atlases, AtlasArray, Atlas2d}` | 30-layer block diffuse + normal D2Array (32×960 PNGs), single-2D block noise, 10 UI textures. `Nearest` sampler, `ClampToEdge`. |
| C4 uniforms | `gfx::uniforms::{FrameUniforms, ModelUniforms, FilterUniforms, UniformBuffer<T>}` | `#[repr(C)]` Pod structs with explicit `_pad`; size-mod-16 compile-time asserts. |
| C5 glyphon text | `gfx::text::TextRenderer` | glyphon 0.11 wrapper. Bundles `unicode.ttf` via `include_bytes!`. |

Cargo deps added at `[C]`: `winit = "0.30"`, `wgpu = "29.0.1"`,
`pollster = "0.4"`, `raw-window-handle = "0.6"`, `image = "0.25"`,
`glyphon = "0.11"`.

### `[D]` world rendering — shipped (`4c060da` on `main`)

| Sub-task | Module | Notes |
|----------|--------|-------|
| D1 mesh | `gfx::mesh::{ChunkVertex, MeshInput, MeshOutput, mesh_chunk}` | Per-face culling against an 18³ padded buffer; 28-byte vertex (`position` + `uv` + `layer` + `face`). Greedy merge intentionally skipped. |
| D2 chunk render | `gfx::chunk_render::{ChunkMesh, ChunkPipeline}` + `gfx::depth::DepthTarget` + `chunk.wgsl` | Two pipelines (opaque + translucent) sharing layout + bind groups; Depth32Float; `coord * CHUNK_SIZE` baked into vertex positions on upload (no per-chunk uniform). |
| D3 particles | `crate::particles::ParticleSystem` (sim) + `gfx::particle_render::*` (GPU) + `particle.wgsl` | C++ gravity (`-0.03/tick`), drag (`0.6/tick`), AABB collision via `BlockView`. 32-byte billboard vertex. |
| D4 final pass | `gfx::chunk.wgsl` (header) | Distance fog (24..96 m linear band) and ambient sky tint folded into the chunk shader rather than spinning up a separate deferred composition pass. |

### `[E]` UI — shipped (`551afba` on `main`)

| Sub-task | Module | Notes |
|----------|--------|-------|
| E1 egui bring-up | `gfx::egui_renderer::EguiRenderer` | egui 0.34 + egui-winit + egui-wgpu; wgpu 29.0.1 compat. `forget_render_pass_lifetime` bridges the `RenderPass<'static>` requirement. |
| E2 screen framework | `ui::screen::{Screen, ScreenStack, Transition}` | `Screen` trait takes `&egui::Context`. `ScreenStack` push/pop/tick. `Transition` has `Push`/`Pop`/`Exit`. |
| E3 menu screens | `ui::screens::{Title,WorldSelect,CreateWorld,Options,Game}` | All five screens implemented as `Screen` trait impls. Title shows "Back to Game" / "Options" / "Quit". Create world has name + seed form. Options sets FOV (70–120), render distance (3–15), VSync, font scale, language. |
| E4 HUD overlay | `ui::hud::Hud` | Crosshair (`Area` at screen center), debug panel (F3 toggle, `Window` with position/yaw/pitch/FPS/chunks), chat bar (T or `/` toggle, `Panel::bottom`). |
| E5 inventory | `ui::inventory::Inventory` | 4×10 slot grid in a centered `Window`. Each slot is a 32×32 px grey square with placeholder label + count. |

`GameScreen` composes `Hud` + `Inventory` and is ticked directly by
`App::frame` when the screen stack is empty; menu screens overlay via the
stack. Cursor grab is driven by stack depth and pause state. The old
glyphon debug-text line is replaced by an egui `Panel::top` status bar.

Cargo deps added at `[E]`: `egui = "0.34"`, `egui-winit = "0.34"`,
`egui-wgpu = "0.34"`.

### `[MVP]` minimum viable game — shipped (`039fe80` on `main`)

`crate::game::Game` owns the World, free-fly Camera, chunk meshes, and
pipelines. Highlights:

* Render distance 3 → 343 chunks loaded synchronously at startup; mesh +
  upload all of them; render every frame.
* World is anchored at world `y = 128` (so chunks span `y ∈ [80, 191]`,
  covering the surface at `y ≈ 120`). Camera spawns at `(0, 160, 32)`.
* World data lives under `std::env::temp_dir()/neworld-mvp/` (sled DB) so
  the binary leaves no artefacts in the launch directory.
* WSAD + Space/Shift movement at 18 m/s (5× with Ctrl). Mouse-look at
  0.0025 rad/px. Click the window to capture the cursor; Esc to release.
* HUD shows position, yaw/pitch, chunk count, FPS, and the control hints.

### Post-MVP fixes (`b5a1239`, `6922b65`)

The first run of the MVP looked wrong in three ways the user caught;
each fix is a targeted patch on top of `[D]` + MVP:

* **Atlas layer order (`b5a1239`)**: the C++ `render::load_png_image`
  Y-flips the PNG during decode (`image.ixx:197` walks rows from
  `height - 1` down to `0`). The diffuse / normal PNGs were therefore
  authored with the LAST atlas entry at the top of the file — TNT at the
  top, WHITE at the bottom — and our Rust `image` crate loads top-down.
  Fix: read PNG block `layers - 1 - texture_layer` for each texture
  array slice. Without this every block sampled an off-by-19 atlas
  layer (rock came out as leaves, etc.).
* **Alpha test (`b5a1239`)**: chunk shader changed `discard if a < 0.5`
  to `<= 0.0`, mirroring C++ `opaque.fsh`. The stricter threshold was
  throwing away water entirely.
* **World center / camera (`b5a1239`)**: anchored chunk grid at world
  `y = 128` (was `y = 0`) so the loaded chunks actually cover the
  generator surface. Camera far plane bumped 256 → 1024.
* **Side-face V flip (`6922b65`)**: chunk vertex UVs now use
  `[(0,1), (1,1), (1,0), (0,0)]` instead of `[(0,0), (1,0), (1,1), (0,1)]`
  to compensate for wgpu's `t = 0` being at the top of the texture data
  (vs OpenGL's bottom). Without this, anisotropic side textures
  (grass-side, wood-side) rendered upside-down — dirt on top, grass on
  bottom.

## Deviations from the migration plan

* **Worldgen seed currently unused.** The C++ `noise_2d` doesn't actually
  mix the per-world `_seed` into its hash; the Rust port is a faithful
  1:1 copy and inherits the bug. Fix is a worldgen-internal change (use a
  `wrapping_mul`-friendly Wang-style mix); flagged with a `TODO`.
* **No async chunk pipeline.** The plan describes a `crossbeam_channel`
  load/save/mesh pipeline (`[F5]` / `[F6]`). MVP ships a synchronous
  variant: at startup `Game::new` pumps `World::tick_chunk_loading` until
  every chunk in `RENDER_DISTANCE` is loaded, then meshes them all in one
  pass. The async layer can replace this without touching the rendering
  API.
* **Chunks are static.** `Game::new` meshes once and keeps the GPU
  buffers for the lifetime of the App. Block changes wouldn't refresh
  the mesh — a deliberate cut for MVP, since `[F]` adds the raycast +
  block-break path that would require remeshing.
* **`World::tick_chunk_loading` `chdir`s to a temp dir at startup.** sled
  opens `worlds/<name>/chunks.db` relative to cwd; `Game::new` calls
  `ensure_world_root()` which creates a temp directory and `chdir`s into
  it before constructing the `World`. Future refactor: thread an
  explicit base path through `TilesStore::open_at`.
* **Commands tests are dispatch-only.** B5 stub-world tests were dropped
  during the merge with the real `World`; per-command behaviour is
  exercised via `world::tests` and `player::tests`. Re-add when there's
  an in-memory `World::new_in_memory`.
* **`ChunkSlot` collocation deferred.** Plan says `Slab<ChunkSlot { chunk,
  render }>`. `[B]` ships plain `Slab<Chunk>`; the GPU-side `ChunkMesh`
  lives in a separate `Vec<ChunkMesh>` on `Game`. The two would join
  when `[F5]` ramps up the async meshing pipeline (so an unloaded chunk
  cleanly drops its GPU buffer too).
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
  When sub-block animation (waving water/leaves) lands, this becomes
  a small group(2) uniform with dynamic offset.
* **No deferred renderer / final pass.** `[D4]` was simplified to
  "distance fog + ambient sky inside the chunk shader" rather than a
  full G-buffer + composition pass. Reversed-Z, shadow maps, SSR, and
  volumetric clouds are all skipped.
* **Save format break.** No backward compatibility with C++ saves.
  Player and chunk files are tagged with `u32` magic + `u32` version
  from day one for future Rust-to-Rust upgrades.
* **egui 0.34 chosen over 0.31 for wgpu 29 compat.** egui-wgpu 0.34.1 is
  the first release that shares wgpu 29.0.1 with the project. The `unsafe`
  lifetime-forget transmute in `egui_renderer.rs` bridges the
  `RenderPass<'static>` requirement in egui-wgpu — the pass does not
  actually borrow anything with its lifetime parameter (see wgpu#1671).

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
* **`unsafe_code = "deny"`** at the crate level; verified by every
  compile.
* **`Pod` types** (`Id`, `State`, `Light`, `BlockData`, `TextureIndex`,
  `ItemStack`, `ChunkVertex`, uniform structs) are `#[repr(C)]` and
  avoid implicit padding (explicit `_pad` where needed).
* **`BaseBlocks` passed explicitly** to every accessor that needs the
  `air` id. The C++ globals (`base_blocks()`,
  `block_info_registry()`) are deliberately not ported.
* **Test harness pattern.** Each module that needs filesystem scratch
  space defines a local `ScratchDir { path, [prev_cwd] }` helper rooted
  in `std::env::temp_dir()`, with `Drop` doing best-effort cleanup. We
  do not take a `tempfile` dependency.
* **wgpu top-down texture convention is honored end-to-end.** The atlas
  uploader reverses the layer order (per the C++ Y-flip-on-load) and the
  chunk mesh flips the V coordinate, so the visual top of every per-block
  art square ends up at the visual top of every face when rendered.

## Open work (not yet started)

Per `rust_migration.md` §5, with the partial ordering:

* **`[F]`** orchestration — `GameApp` root, fixed-step game loop, block
  raycast + breaking, chat input, screenshots, async chunk pipeline,
  end-to-end smoke test. Today's `App` is a hand-rolled variable-step loop
  with no save/load + no per-tick world mutation beyond particle ticking
  (and the ParticleSystem is empty).

Smaller follow-ups inside the layers that ARE shipped:

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
* **`World::TilesStore::open_at(&Path)`** — thread an explicit base path
  through so tests + the MVP no longer have to `chdir`.

## Repository state

* `main` is at `551afba`, all of `[A] + [B] + [C] + [D] + [E] + MVP +
  post-MVP fixes` in linear history, seven squashed feature commits + two
  surgical fix commits on top of the migration plan + initial progress log.
* Worktrees from agent runs live under `.claude/worktrees/`. They are
  locked by the harness while sessions are open and are reaped on
  session end.

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
* Tests: 102 unit tests passing. No integration tests yet.
* Repository on-disk layout under `rs/src/` follows the C++ package structure
  and uses `mod.rs` everywhere.

## Layout (after [B])

```
rs/src/
├── lib.rs                                 # crate root, declares modules
├── main.rs                                # tracing init + placeholder
├── setup.rs                               # tracing init helpers
│
├── blocks.rs                              # Id, State, Light, BlockData,
│                                          #   TextureIndex, BlockInfo,
│                                          #   BlockRegistry, BaseBlocks
├── config.rs                              # TOML options
├── i18n.rs                                # one-TOML-per-language tables
├── input.rs                               # InputState + bitsets (no winit)
├── items.rs                               # ItemStack
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
├── worldgen.rs                            # Generator + HeightMap (small,
│                                          #   not yet split)
│
└── worlds/                                # mirrors C++ src/worlds/
    ├── mod.rs                             # re-exports World + Player
    ├── world/
    │   ├── mod.rs                         # World struct + impl + tests
    │   ├── grid.rs                        # ChunkGrid + ChunkKey
    │   ├── store.rs                       # TilesStore (sled)
    │   └── error.rs                       # WorldError (thiserror)
    └── player/
        ├── mod.rs                         # Player struct + physics + tests
        └── save.rs                        # save_to / load_from + PlayerError
```

## Migration plan tasks shipped

The plan splits work into seven groups (`[A]` through `[F]`). So far, `[A]`
foundations and `[B]` world model are complete.

### `[A]` foundations — shipped (`e2c5a56` on `main`)

| Sub-task | Module | Notes |
|----------|--------|-------|
| A1 skeleton | `lib.rs`, `main.rs`, `setup.rs`, `Cargo.toml`, `assets/` | Single-crate, tracing init, GPL→CC0. |
| A2 math | `math::{aabb, euler, frustum, …}` | `cgmath` (chosen over `glam` for true scalar generics). `Aabb3<S>`, `Euler<S>`, `Frustum<S>`. |
| A3 config | `config::Config` | TOML at `configs/options.toml`. Atomic save (`.tmp`+rename). 18 fields. |
| A4 i18n | `i18n::I18n` | One TOML per language at `assets/lang/<code>.toml`. `get` returns `""` on miss without inserting. |
| A5 input | `input::InputState` | Pure data, no winit dep. `Key` / `MouseButton` enums + bitsets. `begin_frame()` clears per-frame transients. |
| A6 blocks | `blocks::*` | `Pod` newtypes (`Id`, `State`, `Light`, `BlockData`); `TextureIndex` constants folded into `BlockInfo::faces`. `register_base_blocks` populates 19 base blocks. |
| A7 items | `items::ItemStack` | `#[repr(C)]` + `Pod` (with explicit `_pad: u8`). `merge_into` helper for inventory math. |

### `[B]` world model — shipped (`5b917d1` on `main`)

| Sub-task | Module | Notes |
|----------|--------|-------|
| B1 worldgen | `worldgen::{Generator, HeightMap}` | Direct port of the C++ noise math (the C++ `noise_2d` doesn't actually consume the seed; flagged TODO). HeightMap takes `&Generator` per `get(coord, …)` call. |
| B2 chunks | `chunks::{Chunk, ChunkError}` | Lazy `Box<[BlockData; 4096]>`. `block`/`block_mut` take explicit `&BaseBlocks` (no globals). Save format: `NEWC` magic + `u32` version + flags + cells. |
| B3 player | `worlds::player::{Player, GameMode, PlayerError}` | `validate_block_placement` is a pure predicate; the caller does the world write. `save_to`/`load_from` is bincode v2 with `NEWP` magic + version. |
| B4 world | `worlds::world::{World, ChunkGrid, ChunkKey, TilesStore, BlockView, WorldError}` | Slab arena + `by_coord` map + sliding `ChunkGrid` (per plan §2.2/§4.6). Insert/remove keep all three structures consistent; remove clears the grid before freeing the slot (§2.5). `TilesStore` is sled-backed. Meshing is **not** included (deferred to `[D]`). |
| B5 commands | `commands::{Command, CommandRegistry, register_base_commands}` | All 12 C++ slash-commands ported. `/time` is wired through (the C++ left it as a no-op TODO). Deterministic `try_auto_complete`. |

### `[B]` deviations from the migration plan

* **Worldgen seed currently unused.** The C++ `noise_2d` doesn't actually mix
  the per-world `_seed` into its hash; the Rust port is a faithful 1:1 copy
  and inherits the bug. Fix is a worldgen-internal change (use a
  `wrapping_mul`-friendly Wang-style mix); flagged with a `TODO`.
* **No async chunk pipeline.** The migration plan describes a
  `crossbeam_channel`-driven async load/save/mesh pipeline (`[F5]`/`[F6]`).
  `[B]` ships a synchronous variant: `World::tick_chunk_loading` does up to
  `MAX_CHUNK_LOADS` per call inline. The async layer can replace this without
  touching the public API.
* **`World::tick_chunk_loading` `chdir`s in tests.** sled opens
  `worlds/<name>/chunks.db` relative to cwd; the test harness uses a
  per-test `ScratchDir` + a process-global `Mutex<()>` to serialise. Future
  refactor: thread an explicit base path through `TilesStore::open_at`.
* **Commands tests are dispatch-only.** The B5 stub world tests (recording
  `set_block`/`build_tree`/`explode`) were dropped during the merge with the
  real `World` because constructing one requires a sled DB. The dispatch
  logic is still covered (`try_auto_complete`, registration count, duplicate
  panic); per-command behaviour is exercised via `world::tests` and
  `player::tests`. Re-add when there's an in-memory `World::new_in_memory`.
* **`ChunkSlot` collocation deferred.** The plan says `Slab<ChunkSlot {
  chunk, render }>`. `[B]` uses plain `Slab<Chunk>`; the `ChunkRender` half
  joins when `[D]` adds meshing. The `chunk` / `chunk_mut` helpers are the
  only call sites that need to change.
* **Save format break.** No backward compatibility with C++ saves. Player and
  chunk files are tagged with `u32` magic + `u32` version from day one for
  future Rust-to-Rust upgrades.

## Open work (not yet started)

Per `rust_migration.md` §5, with the partial ordering:

* **`[C]`** graphics core — winit + wgpu bring-up, WGSL ports, `glyphon` text.
  Independent of `[B]`; can start now.
* **`[D]`** world rendering — depends on `[B]` + `[C]`. Switches
  `Slab<Chunk>` → `Slab<ChunkSlot { chunk, render }>` and adds the meshing
  pipeline + particle system.
* **`[E]`** UI — egui + screen stack + menus + HUD + inventory. Depends on
  `[A]` + `[C]`.
* **`[F]`** orchestration — `GameApp` root, fixed-step game loop, block
  raycast / breaking, chat input, screenshots, async chunk pipeline,
  end-to-end smoke test. Depends on `[B]` + `[D]` + `[E]`.

## Conventions established along the way

* **mod.rs everywhere.** Even single-file modules get
  `<name>/mod.rs`-on-its-own when they have submodules; flat `<name>.rs` is
  reserved for leaves with no submodules.
* **Reflect C++ packaging.** `rs/src/worlds/{world,player}/…` mirrors
  `src/worlds/{worlds,player,…}.ixx`. Top-level Rust modules correspond to
  top-level C++ `.ixx` files.
* **No backward compatibility with C++ saves or option files.** All formats
  are tagged with magic+version from the start.
* **Each persistent format gets a 4-byte ASCII magic and a `u32` version.**
  `NEWC` for chunks, `NEWP` for player.
* **`unsafe_code = "deny"`** at the crate level; verified by every compile.
* **`Pod` types** (`Id`, `State`, `Light`, `BlockData`, `TextureIndex`,
  `ItemStack`) are `#[repr(C)]` and avoid padding (explicit `_pad` where
  needed) so chunk/inventory blobs round-trip via `bytemuck::cast_slice`.
* **`BaseBlocks` passed explicitly** to every accessor that needs the `air`
  id. The C++ globals (`base_blocks()`, `block_info_registry()`) are
  deliberately not ported.
* **Test harness pattern.** Each module that needs filesystem scratch space
  defines a local `ScratchDir { path, [prev_cwd] }` helper rooted in
  `std::env::temp_dir()`, with `Drop` doing best-effort cleanup. We do not
  take a `tempfile` dependency.

## Repository state

* `main` is at `5b917d1` ahead of `origin/main`. Pushing this commit ships
  all of `[A]` + `[B]`.
* Worktrees from agent runs live under `.claude/worktrees/`. They are locked
  by the harness while sessions are open and are reaped on session end.

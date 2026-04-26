# NEWorld

NEWorld is an open-source voxel game with Minecraft-like gameplay. This
repository hosts the **Rust rewrite**, ported from the original C++23 code base
(preserved under [`old/`](old/) for reference and feature-parity diffing — see
[`old/README.md`](old/README.md) for its build instructions).

The current Rust + C++ designs, a module-by-module feature-parity report,
and the roadmap for closing the remaining gaps all live in
[`docs/rust_migration.md`](docs/rust_migration.md).

## Building

The Rust port is a single Cargo crate. It targets Rust 2024 edition (Rust
1.95 +). All graphics, windowing, and audio dependencies are pure-Rust crates;
no external system libraries (vcpkg, OpenGL loader, FreeType, …) are needed.

```sh
cargo run --release
```

The first build downloads dependencies via crates.io and compiles them; it
takes a few minutes. Subsequent builds are incremental.

To run with a debug build (faster compile, slower runtime):

```sh
cargo run
```

To run the test suite (~150 tests across the lib + integration suites):

```sh
cargo test
```

To run lints (matches the project's CI lint set):

```sh
cargo clippy --all-targets -- -D warnings
```

## File structure

```
.                                   ← Rust crate root (this README)
├── Cargo.toml / Cargo.lock         ← package manifest + locked deps
├── src/                            ← Rust sources (mirror C++ src/ where
│                                     possible — see docs/rust_migration.md
│                                     §1.2 / §2 for the C++ ↔ Rust mapping)
├── shaders/                        ← WGSL shader sources
├── assets/                         ← textures, fonts, language tables
├── tests/                          ← integration test binaries
├── docs/                           ← migration plan + progress log
└── old/                            ← original C++23 implementation (frozen
                                      for diffing; not built by `cargo`)
```

## Status

**All seven groups of the migration plan (`[A]` – `[F]`) have shipped.** The
binary is playable end-to-end: title → world select → in-game with chunk
streaming, player physics, mouse-driven inventory, async load/save, and
screenshot capture. See [`docs/rust_migration.md`](docs/rust_migration.md)
for the parity report plus the roadmap for open polish items (greedy
meshing, smooth lighting, worldgen seed wiring, …).

//! `NEWorld` — Rust port.
//!
//! See `docs/rust_migration.md` (in the repo root) for the migration plan.
//!
//! Top-level layout mirrors the C++ `src/` package one-for-one wherever the
//! mapping is direct, so a feature can be compared between builds with a
//! plain `diff src/<file>.ixx rs/src/<file>.rs`. The Rust-only bits
//! (graphics context, async pipelines, egui bridge, etc.) are grouped under
//! the closest C++ neighbour.
//!
//! Direct C++ ↔ Rust correspondences:
//!
//! | C++ file                            | Rust module                    |
//! |-------------------------------------|--------------------------------|
//! | `blocks.ixx`                        | [`blocks`]                     |
//! | `chunks.ixx`                        | [`chunks`]                     |
//! | `commands.ixx`                      | [`commands`]                   |
//! | `globalization.ixx`                 | [`globalization`]              |
//! | `globals.ixx` (options part)        | [`config`]                     |
//! | `height_maps.ixx`                   | [`height_maps`]                |
//! | `items.ixx`                         | [`items`]                      |
//! | `math/*.ixx`                        | [`math`]                       |
//! | `menus/*.cpp` + `menus.ixx`         | [`menus`]                      |
//! | `particles.ixx`                     | [`particles`]                  |
//! | `render/*.ixx` + `rendering.ixx`    | [`render`]                     |
//! | `setup.ixx`                         | [`setup`]                      |
//! | `terrain_generation.ixx`            | [`terrain_generation`]         |
//! | `text_rendering.ixx`                | [`text_rendering`]             |
//! | `textures.ixx`                      | [`textures`]                   |
//! | `worlds/*.ixx` + `*.cpp`            | [`worlds`]                     |
//! | `neworld.ixx` (window loop part)    | [`app`] + [`game`] (split)     |

pub mod app;
pub mod blocks;
pub mod chunks;
pub mod commands;
pub mod config;
pub mod game;
pub mod globalization;
pub mod height_maps;
pub mod input;
pub mod items;
pub mod math;
pub mod menus;
pub mod particles;
pub mod render;
pub mod setup;
pub mod terrain_generation;
pub mod text_rendering;
pub mod textures;
pub mod ui;
pub mod worlds;

//! Menus — direct mirror of `src/menus/` in the C++ tree.
//!
//! Each file maps one-to-one with the matching C++ source so a feature can
//! be diffed across builds:
//!
//! | C++ file                       | Rust file                 |
//! |--------------------------------|---------------------------|
//! | `main_menu.cpp`                | [`main_menu`]             |
//! | `world_menu.cpp`               | [`world_menu`]            |
//! | `create_world_menu.cpp`        | [`create_world_menu`]     |
//! | `options_menu.cpp`             | [`options_menu`]          |
//! | `game_menu.cpp` (pause)        | [`game_menu`]             |
//!
//! Note: in C++ the in-game HUD and inventory rendering live inside
//! `neworld.ixx`'s `draw_hud` / `draw_inventory` functions, so the Rust
//! `game_menu.rs` covers a slightly larger surface than its C++ counterpart
//! (it composes the HUD + inventory + pause menu into one in-game screen).
//! The pause-menu UI alone matches the C++ `menus/game_menu.cpp` shape.

pub mod create_world_menu;
pub mod game_menu;
pub mod main_menu;
pub mod options_menu;
pub mod world_menu;

pub use create_world_menu::CreateWorldScreen;
pub use game_menu::GameScreen;
pub use main_menu::TitleScreen;
pub use options_menu::OptionsScreen;
pub use world_menu::WorldSelectScreen;

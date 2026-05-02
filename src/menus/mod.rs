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
//! | `render_options_menu.cpp`      | [`render_options_menu`]   |
//! | `shader_options_menu.cpp`      | [`shader_options_menu`]   |
//! | `language_menu.cpp`            | [`language_menu`]         |
//! | `game_menu.cpp` (pause)        | [`game_menu`]             |
//!
//! Note: the Rust `game_menu.rs` covers a slightly larger surface than its
//! C++ counterpart — it composes the in-game HUD ([`crate::game::hud`]),
//! the inventory ([`crate::game::inventory`]), and the pause overlay into
//! one in-game `tick`. The pause-menu UI alone matches the C++
//! `menus/game_menu.cpp` shape.
//!
//! ## Layout chrome
//!
//! Every C++ menu sits inside an identical outer chrome — a centred column
//! padded by 32 px with a maximum width of 512 px and standard 32-px row
//! heights. The constants below are the source of truth shared across all
//! the screens; the actual layout is built per-screen against
//! [`crate::ui::widgets`] (Padding/Sizer/Aligned/Flex/Column/Row) so the
//! shape composes the same way for every menu without hand-rolled chrome
//! helpers.
//!
//! Lifecycle: out-of-game screens implement [`Screen`] and live in a
//! [`ScreenStack`]; the in-game `GameScreen` runs alongside the live
//! `Game` and uses [`crate::ui::widgets::run_overlay`] for its pause
//! overlay so the world keeps rendering behind it.

pub mod action;
pub mod create_world_menu;
pub mod game_menu;
pub mod language_menu;
pub mod main_menu;
pub mod options_menu;
pub mod render_options_menu;
pub mod screen;
pub mod shader_options_menu;
pub mod world_menu;

pub use action::{WorldAction, WorldActionQueue, default_worlds_root};
pub use create_world_menu::CreateWorldScreen;
pub use game_menu::GameScreen;
pub use language_menu::LanguageScreen;
pub use main_menu::TitleScreen;
pub use options_menu::OptionsScreen;
pub use render_options_menu::RenderOptionsScreen;
pub use screen::{Screen, ScreenStack, Transition};
pub use shader_options_menu::ShaderOptionsScreen;
pub use world_menu::WorldSelectScreen;

use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use crate::config::Config;
use crate::globalization::I18n;

/// Build the initial screen stack with the title screen at the bottom. The
/// app starts here on first launch, with no world loaded behind it; clicking
/// "Singleplayer" descends into the world select / create flow.
pub fn initial_screen_stack(
    config: Arc<Mutex<Config>>,
    i18n: Arc<Mutex<I18n>>,
    worlds_root: PathBuf,
    actions: Arc<WorldActionQueue>,
    title_icon: egui::TextureId,
    title_icon_size: (u32, u32),
) -> ScreenStack {
    let mut stack = ScreenStack::new();
    stack.push(Box::new(TitleScreen::new(
        config,
        i18n,
        worlds_root,
        actions,
        title_icon,
        title_icon_size,
    )));
    stack
}

// ---------------------------------------------------------------------------
// Layout constants — every menu pins to the same outer chrome shape.
// ---------------------------------------------------------------------------

/// Outer column max width. Matches the C++ `Sizer({.max_width = 512})`.
pub const MENU_MAX_WIDTH: f32 = 512.0;
/// Outer padding. Matches `Padding({.left=32,.top=32,.right=32,.bottom=32})`.
pub const MENU_PADDING: f32 = 32.0;
/// Standard row height. Matches `Sizer({.max_height = 32})`.
pub const MENU_ROW_HEIGHT: f32 = 32.0;
/// Inter-row vertical spacing. Matches `Spacer({.height = 8})`.
pub const MENU_ROW_SPACING: f32 = 8.0;
/// Inter-column spacing inside a paired row. Matches `Spacer({.width = 8})`.
pub const MENU_COL_SPACING: f32 = 8.0;

/// Snapshot helper: lock `i18n`, copy out a `String` for `key`. Used by
/// every screen so the menu builder doesn't have to hold the mutex past
/// the moment of lookup (matches C++ `GetStrbyKey` semantics).
pub fn t(i18n: &Arc<Mutex<I18n>>, key: &str) -> String {
    i18n.lock()
        .map(|i| i.get(key).to_owned())
        .unwrap_or_default()
}

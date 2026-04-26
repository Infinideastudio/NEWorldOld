//! UI infrastructure — Rust-specific glue around egui.
//!
//! The C++ build has its own `ui/` package (a custom widget toolkit:
//! `Button`, `Label`, `Stack`, …); we use egui instead, so the Rust `ui/`
//! is intentionally smaller and only contains the bits that don't fit
//! elsewhere:
//!
//! * [`screen`] — `Screen` trait + `ScreenStack` push/pop model. Closest
//!   C++ analog is the modal `Menu::run()` blocking call in `ui/element.ixx`.
//! * [`hud`] — in-game HUD overlay (crosshair, debug panel, chat bar). C++
//!   does this inline in `neworld.ixx::draw_hud`.
//! * [`inventory`] — inventory grid + hotbar. C++ does this inline in
//!   `neworld.ixx::draw_inventory`.
//! * [`action`] — `WorldActionQueue`: out-of-band lifecycle requests from
//!   menu screens up to the App. Rust-specific (C++ uses blocking modal
//!   menus that mutate global state directly).
//!
//! The actual menu screens live next door under [`crate::menus`], mirroring
//! the C++ `src/menus/` directory.

pub mod action;
pub mod hud;
pub mod inventory;
pub mod screen;

use std::path::PathBuf;
use std::sync::atomic::AtomicBool;
use std::sync::{Arc, Mutex};

use crate::config::Config;
use crate::menus::TitleScreen;

pub use action::{WorldAction, WorldActionQueue, default_worlds_root};
pub use screen::{Screen, ScreenStack, Transition};

/// Build the initial screen stack with the title screen at the bottom. The
/// app starts here on first launch, with no world loaded behind it; clicking
/// "Singleplayer" descends into the world select / create flow.
#[must_use]
pub fn initial_screen_stack(
    config: Arc<Mutex<Config>>,
    worlds_root: PathBuf,
    actions: Arc<WorldActionQueue>,
    game_loaded: Arc<AtomicBool>,
) -> ScreenStack {
    let mut stack = ScreenStack::new();
    stack.push(Box::new(TitleScreen::new(
        config,
        worlds_root,
        actions,
        game_loaded,
    )));
    stack
}

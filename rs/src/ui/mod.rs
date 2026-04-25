//! UI layer — immediate-mode UI on top of egui ([E] in `docs/rust_migration.md`).
//!
//! Built on egui 0.34. Contains:
//! * [`screen`] — `Screen` trait + `ScreenStack` push/pop model (E2).
//! * [`screens`] — menu screens: title, world select, create world, options, game (E3).
//! * [`hud`] — in-game HUD overlay: crosshair, debug panel, chat bar (E4).
//! * [`inventory`] — inventory grid overlay (E5).

pub mod hud;
pub mod inventory;
pub mod screen;
pub mod screens;

use std::sync::{Arc, Mutex};

use crate::config::Config;

pub use screen::{Screen, ScreenStack, Transition};
pub use screens::GameScreen;

/// Create the initial screen stack with the title screen as the entry point.
/// `config` is the live app configuration; menu screens edit it in place
/// and the app re-applies the live-applicable values each frame.
#[must_use]
pub fn initial_screen_stack(config: Arc<Mutex<Config>>) -> ScreenStack {
    let mut stack = ScreenStack::new();
    stack.push(Box::new(screens::TitleScreen::new(config)));
    stack
}

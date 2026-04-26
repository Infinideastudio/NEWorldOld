//! UI layer — immediate-mode UI on top of egui ([E] in `docs/rust_migration.md`).
//!
//! Built on egui 0.34. Contains:
//! * [`screen`] — `Screen` trait + `ScreenStack` push/pop model (E2).
//! * [`screens`] — menu screens: title, world select, create world, options, game (E3).
//! * [`hud`] — in-game HUD overlay: crosshair, debug panel, chat bar (E4).
//! * [`inventory`] — inventory grid overlay (E5).
//! * [`action`] — out-of-band lifecycle requests from screens up to the App
//!   (open world, leave to title, delete world). See [`action::WorldAction`].

pub mod action;
pub mod hud;
pub mod inventory;
pub mod screen;
pub mod screens;

use std::path::PathBuf;
use std::sync::atomic::AtomicBool;
use std::sync::{Arc, Mutex};

use crate::config::Config;

pub use action::{WorldAction, WorldActionQueue, default_worlds_root};
pub use screen::{Screen, ScreenStack, Transition};
pub use screens::GameScreen;

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
    stack.push(Box::new(screens::TitleScreen::new(
        config,
        worlds_root,
        actions,
        game_loaded,
    )));
    stack
}

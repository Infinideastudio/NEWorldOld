//! Title screen — the main menu shown at startup and on "Quit to title".
//!
//! Holds an `Arc<AtomicBool>` flag named `game_loaded` so the menu can show
//! "Back to Game" only when the app actually has a live `Game` to return to.
//! Pressing "Singleplayer" pushes the [`super::WorldSelectScreen`] which lists
//! every world in `<root>/worlds/`.

use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};

use egui::Context;

use super::super::action::WorldActionQueue;
use super::super::screen::{Screen, Transition};
use super::{OptionsScreen, WorldSelectScreen};
use crate::config::Config;

/// The main title screen. Always sits at the bottom of the screen stack
/// when no world is loaded; pushed back on top from the in-game pause menu
/// after `WorldAction::LeaveToTitle` has torn the world down.
pub struct TitleScreen {
    /// Shared with the App + every other screen — `Options` mutates it
    /// directly.
    config: Arc<Mutex<Config>>,
    /// Where the world list and "create world" screen anchor their disk I/O.
    /// Threaded through so the screens don't have to duplicate the dev-vs-
    /// release path resolution from [`super::super::action::default_worlds_root`].
    worlds_root: PathBuf,
    /// Mailbox for cross-screen world-lifecycle requests (open / leave /
    /// delete). The world-select screen sends `Enter`; the app drains.
    actions: Arc<WorldActionQueue>,
    /// Whether a `Game` is currently live. When `true`, the title can pop
    /// itself to return to the game; when `false`, "Back to Game" is hidden.
    game_loaded: Arc<AtomicBool>,
}

impl TitleScreen {
    #[must_use]
    pub fn new(
        config: Arc<Mutex<Config>>,
        worlds_root: PathBuf,
        actions: Arc<WorldActionQueue>,
        game_loaded: Arc<AtomicBool>,
    ) -> Self {
        Self {
            config,
            worlds_root,
            actions,
            game_loaded,
        }
    }
}

impl Screen for TitleScreen {
    fn title(&self) -> &'static str {
        "Title"
    }

    #[allow(deprecated)] // CentralPanel::show
    fn ui(&mut self, ctx: &Context) -> Transition {
        let mut transition = Transition::None;
        let game_loaded = self.game_loaded.load(Ordering::Relaxed);

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.vertical_centered(|ui| {
                ui.add_space(120.0);

                ui.heading("NEWorld");
                ui.separator();
                ui.add_space(20.0);

                if game_loaded && ui.button("Back to Game").clicked() {
                    transition = Transition::Pop;
                }

                if ui.button("Singleplayer").clicked() {
                    transition = Transition::Push(Box::new(WorldSelectScreen::new(
                        self.worlds_root.clone(),
                        Arc::clone(&self.actions),
                    )));
                }

                if ui.button("Options").clicked() {
                    transition = Transition::Push(Box::new(OptionsScreen::new(
                        Arc::clone(&self.config),
                    )));
                }

                ui.add_space(10.0);

                if ui.button("Quit").clicked() {
                    transition = Transition::Exit;
                }
            });
        });

        transition
    }
}

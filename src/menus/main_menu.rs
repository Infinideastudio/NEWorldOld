//! Title screen — direct mirror of `old/src/menus/main_menu.cpp`.
//!
//! Layout: a 256-px logo banner at the top, a wide "Start game" button, a
//! row pairing "Options" with "Exit", and a help line anchored to the
//! bottom-left corner of the screen. All strings come from the active
//! [`I18n`] table so the layout matches the C++ build verbatim regardless
//! of the configured language.
//!
//! When the app is mid-game ("Quit to title" was clicked but the user wants
//! to come back), an extra "Back to game" full-row button slots in above
//! "Start game".

use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};

use egui::{Align2, Color32, Context, FontId};

use super::{
    OptionsScreen, WorldSelectScreen, full_row_button, menu_panel, pair_row, t, MENU_ROW_SPACING,
};
use crate::config::Config;
use crate::globalization::I18n;
use crate::ui::action::WorldActionQueue;
use crate::ui::screen::{Screen, Transition};

/// The main title screen. Always sits at the bottom of the screen stack
/// when no world is loaded; pushed back on top from the in-game pause menu
/// after `WorldAction::LeaveToTitle` has torn the world down.
pub struct TitleScreen {
    /// Shared with the App + every other screen — `Options` mutates it
    /// directly.
    config: Arc<Mutex<Config>>,
    /// Active language table, threaded into every menu so labels stay in
    /// sync after the language picker reloads.
    i18n: Arc<Mutex<I18n>>,
    /// Where the world list and "create world" screen anchor their disk I/O.
    /// Threaded through so the screens don't have to duplicate the dev-vs-
    /// release path resolution from [`crate::ui::action::default_worlds_root`].
    worlds_root: PathBuf,
    /// Mailbox for cross-screen world-lifecycle requests (open / leave /
    /// delete). The world-select screen sends `Enter`; the app drains.
    actions: Arc<WorldActionQueue>,
    /// Whether a `Game` is currently live. When `true`, the title can pop
    /// itself to return to the game; when `false`, "Back to game" is hidden.
    game_loaded: Arc<AtomicBool>,
}

impl TitleScreen {
    #[must_use]
    pub fn new(
        config: Arc<Mutex<Config>>,
        i18n: Arc<Mutex<I18n>>,
        worlds_root: PathBuf,
        actions: Arc<WorldActionQueue>,
        game_loaded: Arc<AtomicBool>,
    ) -> Self {
        Self {
            config,
            i18n,
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

    fn ui(&mut self, ctx: &Context) -> Transition {
        let mut transition = Transition::None;
        let game_loaded = self.game_loaded.load(Ordering::Relaxed);

        let start_label = t(&self.i18n, "NEWorld.main.start");
        let options_label = t(&self.i18n, "NEWorld.main.options");
        let exit_label = t(&self.i18n, "NEWorld.main.exit");
        let help_label = t(&self.i18n, "NEWorld.main.help");

        menu_panel(ctx, |ui| {
            // 256-px banner at the top — placeholder for the C++ TitleTexture
            // ImageBox until the title PNG is bridged to egui.
            let avail = ui.available_width();
            let (rect, _) = ui.allocate_exact_size(egui::vec2(avail, 256.0), egui::Sense::hover());
            ui.painter().text(
                rect.center(),
                Align2::CENTER_CENTER,
                "NEWorld",
                FontId::proportional(64.0),
                Color32::from_gray(230),
            );
            ui.add_space(MENU_ROW_SPACING);

            if game_loaded && full_row_button(ui, "Back to game") {
                transition = Transition::Pop;
            }
            if game_loaded {
                ui.add_space(MENU_ROW_SPACING);
            }

            if full_row_button(ui, &start_label) {
                transition = Transition::Push(Box::new(WorldSelectScreen::new(
                    self.worlds_root.clone(),
                    Arc::clone(&self.i18n),
                    Arc::clone(&self.actions),
                )));
            }
            ui.add_space(MENU_ROW_SPACING);

            pair_row(ui, |cols| {
                if cols[0].button(&options_label).clicked() {
                    transition = Transition::Push(Box::new(OptionsScreen::new(
                        Arc::clone(&self.config),
                        Arc::clone(&self.i18n),
                    )));
                }
                if cols[1].button(&exit_label).clicked() {
                    transition = Transition::Exit;
                }
            });
        });

        // Help line anchored to the bottom-left of the whole window — mirrors
        // the C++ `StackItem({.alignment = BOTTOM_LEFT}, Padding(...,
        // Label(help)))`.
        if !help_label.is_empty() {
            egui::Area::new("title.help".into())
                .anchor(Align2::LEFT_BOTTOM, egui::vec2(8.0, -8.0))
                .interactable(false)
                .show(ctx, |ui| {
                    ui.label(
                        egui::RichText::new(help_label)
                            .color(Color32::from_gray(200))
                            .size(13.0),
                    );
                });
        }

        transition
    }
}

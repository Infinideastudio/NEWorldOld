//! Title screen — the main menu.

use egui::Context;

use super::super::screen::{Screen, Transition};
use super::OptionsScreen;

/// The main title screen.
///
/// When pushed from the in-game pause menu, "Back to Game" pops this screen
/// to return to gameplay. When the app starts with this screen (no game
/// behind it), "Back to Game" is hidden.
#[derive(Default)]
pub struct TitleScreen;

impl Screen for TitleScreen {
    fn title(&self) -> &'static str {
        "Title"
    }

    #[allow(deprecated)] // CentralPanel::show
    fn ui(&mut self, ctx: &Context) -> Transition {
        let mut transition = Transition::None;

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.vertical_centered(|ui| {
                ui.add_space(120.0);

                ui.heading("NEWorld");
                ui.separator();
                ui.add_space(20.0);

                if ui.button("Back to Game").clicked() {
                    transition = Transition::Pop;
                }

                if ui.button("Options").clicked() {
                    transition = Transition::Push(Box::new(OptionsScreen::default()));
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

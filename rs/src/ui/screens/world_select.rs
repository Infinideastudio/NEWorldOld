//! World selection screen — lists existing worlds and allows creating new ones.

use egui::Context;

use super::super::screen::{Screen, Transition};
use super::CreateWorldScreen;

/// World selection screen.
///
/// For now, shows a placeholder "No worlds found" message since save/load
/// is not yet implemented.
#[derive(Default)]
pub struct WorldSelectScreen;

impl Screen for WorldSelectScreen {
    fn title(&self) -> &'static str {
        "Select World"
    }

    #[allow(deprecated)] // CentralPanel::show
    fn ui(&mut self, ctx: &Context) -> Transition {
        let mut transition = Transition::None;

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.vertical_centered(|ui| {
                ui.add_space(60.0);
                ui.heading("Select World");
                ui.separator();
                ui.add_space(10.0);
            });

            ui.horizontal(|ui| {
                if ui.button("\u{2190} Back").clicked() {
                    transition = Transition::Pop;
                }
                ui.label("   ");
            });
            ui.separator();

            egui::ScrollArea::vertical().show(ui, |ui| {
                ui.vertical_centered(|ui| {
                    ui.add_space(40.0);
                    ui.label("No worlds found");
                    ui.label("Create a new world to get started!");
                    ui.add_space(20.0);

                    if ui.button("Create New World").clicked() {
                        transition = Transition::Push(Box::new(
                            CreateWorldScreen::default(),
                        ));
                    }
                });
            });
        });

        transition
    }
}

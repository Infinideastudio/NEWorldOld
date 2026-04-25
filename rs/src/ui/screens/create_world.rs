//! Create world screen — simple form for world name and optional seed.

use egui::Context;

use super::super::screen::{Screen, Transition};

/// Create world form state.
#[derive(Default)]
pub struct CreateWorldScreen {
    world_name: String,
    seed: String,
}

impl Screen for CreateWorldScreen {
    fn title(&self) -> &'static str {
        "Create World"
    }

    #[allow(deprecated)] // CentralPanel::show
    fn ui(&mut self, ctx: &Context) -> Transition {
        let mut transition = Transition::None;

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.vertical_centered(|ui| {
                ui.add_space(60.0);
                ui.heading("Create New World");
                ui.separator();
                ui.add_space(10.0);
            });

            ui.horizontal(|ui| {
                if ui.button("\u{2190} Cancel").clicked() {
                    transition = Transition::Pop;
                }
            });
            ui.separator();

            egui::Frame::NONE.show(ui, |ui| {
                ui.vertical_centered(|ui| {
                    ui.add_space(20.0);

                    ui.horizontal(|ui| {
                        ui.label("World Name: ");
                        ui.text_edit_singleline(&mut self.world_name);
                    });

                    ui.add_space(10.0);

                    ui.horizontal(|ui| {
                        ui.label("Seed (optional): ");
                        ui.text_edit_singleline(&mut self.seed);
                    });

                    ui.add_space(20.0);

                    if ui.button("Create").clicked() {
                        transition = Transition::Pop;
                    }
                });
            });
        });

        transition
    }
}

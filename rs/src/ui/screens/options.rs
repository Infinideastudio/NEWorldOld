//! Options screen — settings panel (FOV, render distance, `VSync`, etc.).

use egui::Context;

use super::super::screen::{Screen, Transition};

/// Settings state. Will eventually be wired to `Config`.
pub struct OptionsScreen {
    fov: f32,
    render_distance: u32,
    vsync: bool,
    font_scale: f32,
    language: String,
}

impl Default for OptionsScreen {
    fn default() -> Self {
        Self {
            fov: 70.0,
            render_distance: 3,
            vsync: true,
            font_scale: 1.0,
            language: "English".to_string(),
        }
    }
}

impl OptionsScreen {
    fn clamp_settings(&mut self) {
        self.fov = self.fov.clamp(70.0, 120.0);
        self.render_distance = self.render_distance.clamp(3, 15);
        self.font_scale = self.font_scale.clamp(0.5, 2.0);
    }
}

impl Screen for OptionsScreen {
    fn title(&self) -> &'static str {
        "Options"
    }

    #[allow(deprecated)] // CentralPanel::show
    fn ui(&mut self, ctx: &Context) -> Transition {
        let mut transition = Transition::None;

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.vertical_centered(|ui| {
                ui.add_space(60.0);
                ui.heading("Options");
                ui.separator();
                ui.add_space(10.0);
            });

            ui.horizontal(|ui| {
                if ui.button("\u{2190} Back").clicked() {
                    transition = Transition::Pop;
                }
            });
            ui.separator();

            egui::ScrollArea::vertical().show(ui, |ui| {
                ui.add_space(10.0);

                ui.horizontal(|ui| {
                    ui.label("Field of View");
                    ui.add(
                        egui::Slider::new(&mut self.fov, 70.0..=120.0)
                            .suffix(" deg")
                            .integer(),
                    );
                });

                ui.add_space(8.0);

                ui.horizontal(|ui| {
                    ui.label("Render Distance");
                    ui.add(
                        egui::Slider::new(&mut self.render_distance, 3..=15)
                            .suffix(" chunks"),
                    );
                });

                ui.add_space(8.0);

                ui.horizontal(|ui| {
                    ui.checkbox(&mut self.vsync, "VSync");
                });

                ui.add_space(8.0);

                ui.horizontal(|ui| {
                    ui.label("Font Scale");
                    ui.add(
                        egui::Slider::new(&mut self.font_scale, 0.5..=2.0)
                            .suffix("x"),
                    );
                });

                ui.add_space(8.0);

                ui.horizontal(|ui| {
                    ui.label("Language");
                    egui::ComboBox::from_id_salt("language")
                        .selected_text(&self.language)
                        .show_ui(ui, |ui| {
                            ui.selectable_value(
                                &mut self.language,
                                "English".to_string(),
                                "English",
                            );
                        });
                });

                ui.add_space(20.0);
            });

            self.clamp_settings();
        });

        transition
    }
}

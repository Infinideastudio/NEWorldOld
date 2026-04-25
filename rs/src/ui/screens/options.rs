//! Options screen — settings panel (FOV, render distance, `VSync`, etc.).
//!
//! Edits a shared [`Config`] handle in place. The `App` watches the same
//! `Arc<Mutex<Config>>` and re-applies the live-applicable values to the
//! camera / surface / egui scale every frame; values that need a world
//! restart (currently only render distance) are flagged in the UI.

use std::sync::{Arc, Mutex};

use egui::Context;

use super::super::screen::{Screen, Transition};
use crate::config::Config;

/// Sliders / pickers for the live [`Config`]. The widgets bind directly to
/// the locked struct's fields, so changes take effect the moment the user
/// drags a slider.
pub struct OptionsScreen {
    config: Arc<Mutex<Config>>,
}

impl OptionsScreen {
    #[must_use]
    pub fn new(config: Arc<Mutex<Config>>) -> Self {
        Self { config }
    }
}

impl Screen for OptionsScreen {
    fn title(&self) -> &'static str {
        "Options"
    }

    #[allow(deprecated)] // CentralPanel::show
    fn ui(&mut self, ctx: &Context) -> Transition {
        let mut transition = Transition::None;
        let mut cfg = self.config.lock().expect("config poisoned");

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
                        egui::Slider::new(&mut cfg.fov_y_normal, 70.0..=120.0)
                            .suffix(" deg")
                            .integer(),
                    );
                });

                ui.add_space(8.0);

                ui.horizontal(|ui| {
                    ui.label("Render Distance");
                    ui.add(
                        egui::Slider::new(&mut cfg.render_distance, 3..=15).suffix(" chunks"),
                    );
                });
                ui.colored_label(
                    egui::Color32::from_gray(160),
                    "  (takes effect on next world load)",
                );

                ui.add_space(8.0);

                ui.horizontal(|ui| {
                    ui.label("Mouse Sensitivity");
                    ui.add(egui::Slider::new(&mut cfg.mouse_speed, 0.01..=0.5));
                });

                ui.add_space(8.0);

                ui.horizontal(|ui| {
                    ui.checkbox(&mut cfg.vertical_sync, "VSync");
                });

                ui.add_space(8.0);

                ui.horizontal(|ui| {
                    ui.label("Font Scale");
                    ui.add(egui::Slider::new(&mut cfg.font_scale, 0.5..=2.0).suffix("x"));
                });

                ui.add_space(8.0);

                ui.horizontal(|ui| {
                    ui.label("Language");
                    egui::ComboBox::from_id_salt("language")
                        .selected_text(&cfg.language)
                        .show_ui(ui, |ui| {
                            ui.selectable_value(
                                &mut cfg.language,
                                "en_US".to_string(),
                                "English (en_US)",
                            );
                            ui.selectable_value(
                                &mut cfg.language,
                                "zh_CN".to_string(),
                                "\u{4E2D}\u{6587} (zh_CN)",
                            );
                        });
                });

                ui.add_space(20.0);
            });

            // Clamp anything that might have been edited via TextEdit into a
            // bad range. Sliders already enforce their own bounds, but a
            // future TextEdit-backed widget could overshoot.
            cfg.fov_y_normal = cfg.fov_y_normal.clamp(70.0, 120.0);
            cfg.render_distance = cfg.render_distance.clamp(3, 15);
            cfg.font_scale = cfg.font_scale.clamp(0.5, 2.0);
            cfg.mouse_speed = cfg.mouse_speed.clamp(0.01, 0.5);
        });

        transition
    }
}

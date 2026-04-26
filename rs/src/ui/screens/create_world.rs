//! Create world screen — name + optional seed form.
//!
//! "Create" creates `<root>/worlds/<name>/` on disk (so the entry shows up in
//! the world-select list immediately) and pops back to it. The world isn't
//! actually loaded until the user selects it from the list and clicks Enter
//! — keeping the two-step flow aligned with the C++ menu (which also forces
//! a "back to world list, then click Enter" round trip after Create).

use std::path::PathBuf;
use std::sync::Arc;

use egui::Context;

use super::super::action::WorldActionQueue;
use super::super::screen::{Screen, Transition};

/// Create world form state.
pub struct CreateWorldScreen {
    worlds_root: PathBuf,
    /// Held in case a future iteration wants to immediately submit
    /// `WorldAction::Enter` after creating, instead of bouncing back through
    /// the world list. Today we just pop, so the field is unused at runtime.
    #[allow(dead_code)]
    actions: Arc<WorldActionQueue>,
    world_name: String,
    seed: String,
    /// Set to a human-readable error message if the most recent Create
    /// attempt failed (illegal name, world already exists, fs error). Shown
    /// inline above the form.
    error: Option<String>,
}

impl CreateWorldScreen {
    #[must_use]
    pub fn new(worlds_root: PathBuf, actions: Arc<WorldActionQueue>) -> Self {
        Self {
            worlds_root,
            actions,
            world_name: String::new(),
            seed: String::new(),
            error: None,
        }
    }

    /// Validate `name` and create the world directory if it doesn't already
    /// exist. Returns `Ok(())` on success; populates `self.error` on failure.
    fn try_create(&mut self) -> bool {
        let name = self.world_name.trim();
        if name.is_empty() {
            self.error = Some("World name can't be empty.".to_owned());
            return false;
        }
        if !is_safe_world_name(name) {
            self.error = Some(
                "Use only letters, numbers, dashes and underscores in the world name."
                    .to_owned(),
            );
            return false;
        }
        let dir = self.worlds_root.join("worlds").join(name);
        if dir.exists() {
            self.error = Some("A world with that name already exists.".to_owned());
            return false;
        }
        if let Err(err) = std::fs::create_dir_all(&dir) {
            self.error = Some(format!("Failed to create world directory: {err}"));
            return false;
        }
        self.error = None;
        true
    }
}

impl Screen for CreateWorldScreen {
    fn title(&self) -> &'static str {
        "Create World"
    }

    #[allow(deprecated)] // CentralPanel::show
    fn ui(&mut self, ctx: &Context) -> Transition {
        let mut transition = Transition::None;
        let mut create_clicked = false;
        let mut cancel_clicked = false;

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.vertical_centered(|ui| {
                ui.add_space(40.0);
                ui.heading("Create New World");
                ui.separator();
                ui.add_space(10.0);
            });

            ui.horizontal(|ui| {
                if ui.button("\u{2190} Cancel").clicked() {
                    cancel_clicked = true;
                }
            });
            ui.separator();

            if let Some(msg) = &self.error {
                ui.colored_label(egui::Color32::LIGHT_RED, msg);
                ui.add_space(8.0);
            }

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

            ui.horizontal(|ui| {
                if ui.button("Create").clicked() {
                    create_clicked = true;
                }
            });
        });

        if cancel_clicked || (create_clicked && self.try_create()) {
            transition = Transition::Pop;
        }

        transition
    }
}

/// Restrict world names to a tiny safe alphabet so we never have to worry
/// about path-traversal sequences (`..`), reserved Windows names, or
/// filesystem-unsafe punctuation. Empty input is rejected separately.
fn is_safe_world_name(name: &str) -> bool {
    name.chars()
        .all(|c| c.is_ascii_alphanumeric() || c == '-' || c == '_')
}

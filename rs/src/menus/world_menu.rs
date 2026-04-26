//! World selection screen — lists existing worlds and routes "Enter" /
//! "Delete" through the [`WorldActionQueue`].
//!
//! The list of worlds is read from `<root>/worlds/` via
//! [`crate::worlds::World::list_worlds_at`] and refreshed each time the
//! screen is built (cheap directory scan, only happens when the user opens
//! this screen). The actual world load happens asynchronously in the app:
//! pressing "Enter" submits a `WorldAction::Enter`, then the screen pops
//! itself off the stack so the next frame the title screen is on top, and
//! the app drains the action and loads the world.

use std::path::PathBuf;
use std::sync::Arc;

use egui::Context;

use crate::ui::action::{WorldAction, WorldActionQueue};
use crate::ui::screen::{Screen, Transition};
use super::CreateWorldScreen;
use crate::worlds::World;

/// World selection screen.
pub struct WorldSelectScreen {
    worlds_root: PathBuf,
    actions: Arc<WorldActionQueue>,
    /// Cache of world directory names. Refreshed on construction and after
    /// every "Create" / "Delete" action so the list stays in sync without
    /// re-scanning every frame.
    entries: Vec<String>,
    /// Currently-selected world name, or empty if nothing selected. The
    /// "Enter" / "Delete" buttons act on this.
    selected: String,
    /// Set when the user clicks "Enter" — defers the action submission +
    /// `Pop` until after the egui frame finishes (we can't borrow `self`
    /// inside a closure that's already borrowing it).
    pending_enter: bool,
}

impl WorldSelectScreen {
    #[must_use]
    pub fn new(worlds_root: PathBuf, actions: Arc<WorldActionQueue>) -> Self {
        let entries = World::list_worlds_at(&worlds_root);
        Self {
            worlds_root,
            actions,
            entries,
            selected: String::new(),
            pending_enter: false,
        }
    }

    fn refresh(&mut self) {
        self.entries = World::list_worlds_at(&self.worlds_root);
        if !self.entries.iter().any(|n| n == &self.selected) {
            self.selected.clear();
        }
    }
}

impl Screen for WorldSelectScreen {
    fn title(&self) -> &'static str {
        "Select World"
    }

    #[allow(deprecated)] // CentralPanel::show
    fn ui(&mut self, ctx: &Context) -> Transition {
        // Refresh the cache every frame — the directory scan is O(N) over a
        // small N and runs only while the user is looking at this screen, so
        // there's no measurable cost. Doing it here means a world created
        // through the CreateWorldScreen (which writes to disk and pops back
        // to us) shows up on the very next frame.
        self.refresh();

        let mut transition = Transition::None;
        let mut create_clicked = false;
        let mut delete_clicked = false;
        let mut back_clicked = false;
        let mut enter_clicked = false;

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.vertical_centered(|ui| {
                ui.add_space(40.0);
                ui.heading("Select World");
                ui.separator();
                ui.add_space(10.0);
            });

            ui.horizontal(|ui| {
                if ui.button("\u{2190} Back").clicked() {
                    back_clicked = true;
                }
            });
            ui.separator();

            egui::ScrollArea::vertical()
                .max_height(280.0)
                .show(ui, |ui| {
                    if self.entries.is_empty() {
                        ui.vertical_centered(|ui| {
                            ui.add_space(40.0);
                            ui.colored_label(
                                egui::Color32::from_gray(180),
                                "No worlds yet — create one below.",
                            );
                        });
                    } else {
                        for name in &self.entries {
                            let is_selected = self.selected == *name;
                            let resp = ui.selectable_label(is_selected, name);
                            if resp.clicked() {
                                self.selected = name.clone();
                            }
                            if resp.double_clicked() {
                                self.selected = name.clone();
                                enter_clicked = true;
                            }
                        }
                    }
                });

            ui.separator();

            ui.horizontal(|ui| {
                let has_sel = !self.selected.is_empty();
                if ui
                    .add_enabled(has_sel, egui::Button::new("Enter"))
                    .clicked()
                {
                    enter_clicked = true;
                }
                if ui
                    .add_enabled(has_sel, egui::Button::new("Delete"))
                    .clicked()
                {
                    delete_clicked = true;
                }
                if ui.button("Create New World").clicked() {
                    create_clicked = true;
                }
            });
        });

        if back_clicked {
            transition = Transition::Pop;
        } else if create_clicked {
            transition = Transition::Push(Box::new(CreateWorldScreen::new(
                self.worlds_root.clone(),
                Arc::clone(&self.actions),
            )));
        } else if delete_clicked {
            self.actions.submit(WorldAction::Delete {
                name: self.selected.clone(),
            });
            // Optimistically refresh so the deleted entry vanishes immediately.
            // The app will perform the real fs::remove_dir_all on the next
            // frame; if that fails, the next refresh will re-add it.
            self.refresh();
        } else if enter_clicked || self.pending_enter {
            // Submit the Enter action and pop ourselves so the app sees a
            // clean stack on the next frame.
            self.actions.submit(WorldAction::Enter {
                name: self.selected.clone(),
                seed: derive_seed(&self.selected),
            });
            transition = Transition::Pop;
            self.pending_enter = false;
        }

        transition
    }
}

/// Derive a default per-world seed from the world name. Stable across runs so
/// reopening a world gives the same terrain. The hash is djb2 — adequate for
/// a worldgen seed; not a cryptographic primitive.
fn derive_seed(name: &str) -> u32 {
    let mut h: u32 = 5381;
    for b in name.as_bytes() {
        h = h.wrapping_mul(33).wrapping_add(u32::from(*b));
    }
    h
}

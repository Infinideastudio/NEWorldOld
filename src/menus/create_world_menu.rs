//! Create world screen — direct mirror of `old/src/menus/create_world_menu.cpp`.
//!
//! Layout: caption row, single-line text-edit row for the world name, then a
//! Back / OK pair row. The C++ build creates the world directory inline on
//! "OK" and pops back; we do the same. Validation errors (empty name,
//! illegal characters, name collision) appear above the form in red.

use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use egui::{Color32, Context};

use super::{MENU_ROW_HEIGHT, MENU_ROW_SPACING, caption_row, menu_panel, pair_row, t};
use crate::globalization::I18n;
use crate::ui::action::WorldActionQueue;
use crate::ui::screen::{Screen, Transition};

/// Create world form state.
pub struct CreateWorldScreen {
    worlds_root: PathBuf,
    i18n: Arc<Mutex<I18n>>,
    /// Held in case a future iteration wants to immediately submit
    /// `WorldAction::Enter` after creating, instead of bouncing back through
    /// the world list. Today we just pop, so the field is unused at runtime.
    #[allow(dead_code)]
    actions: Arc<WorldActionQueue>,
    world_name: String,
    /// Set to a human-readable error message if the most recent Create
    /// attempt failed. Shown inline above the form.
    error: Option<String>,
}

impl CreateWorldScreen {
    #[must_use]
    pub fn new(
        worlds_root: PathBuf,
        i18n: Arc<Mutex<I18n>>,
        actions: Arc<WorldActionQueue>,
    ) -> Self {
        Self {
            worlds_root,
            i18n,
            actions,
            world_name: String::new(),
            error: None,
        }
    }

    /// Validate `name` and create the world directory if it doesn't already
    /// exist. Returns `true` on success.
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
        // Path convention: worlds live at `<worlds_root>/worlds/<name>/`,
        // matching `World::new_at`'s internal layout. `worlds_root` is the
        // parent dir (crate dir in dev), not the worlds dir itself.
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

    fn ui(&mut self, ctx: &Context) -> Transition {
        let caption = t(&self.i18n, "NEWorld.create.caption");
        let placeholder = t(&self.i18n, "NEWorld.create.inputname");
        let ok_label = t(&self.i18n, "NEWorld.create.ok");
        let back_label = t(&self.i18n, "NEWorld.create.back");

        let mut transition = Transition::None;
        let mut create_clicked = false;
        let mut cancel_clicked = false;

        menu_panel(ctx, "create_world", |ui| {
            caption_row(ui, &caption);
            ui.add_space(MENU_ROW_SPACING);

            if let Some(msg) = &self.error {
                ui.colored_label(Color32::LIGHT_RED, msg);
                ui.add_space(MENU_ROW_SPACING);
            }

            // Single-line text edit row.
            ui.allocate_ui_with_layout(
                egui::vec2(ui.available_width(), MENU_ROW_HEIGHT),
                egui::Layout::centered_and_justified(egui::Direction::LeftToRight),
                |ui| {
                    ui.add(
                        egui::TextEdit::singleline(&mut self.world_name)
                            .hint_text(&placeholder)
                            .desired_width(f32::INFINITY),
                    );
                },
            );
            ui.add_space(MENU_ROW_SPACING);

            // Footer: Back / OK pair_row, sitting at the natural bottom
            // of the centred body.
            pair_row(ui, |cols| {
                if cols[0].button(&back_label).clicked() {
                    cancel_clicked = true;
                }
                if cols[1].button(&ok_label).clicked() {
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

/// Restrict world names to a safe alphabet so we never have to worry about
/// path-traversal sequences (`..`), reserved Windows names, or unsafe
/// punctuation. Empty input is rejected separately.
fn is_safe_world_name(name: &str) -> bool {
    name.chars()
        .all(|c| c.is_ascii_alphanumeric() || c == '-' || c == '_')
}

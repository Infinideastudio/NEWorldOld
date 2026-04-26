//! World selection screen — direct mirror of `old/src/menus/world_menu.cpp`.
//!
//! Layout: a centred caption row, a scrollable column of one button per
//! world (the C++ build paints the world's `thumbnail.png` behind the name —
//! we just show the name on a button until thumbnail loading is wired into
//! egui), a "Create new world" button, a flex spacer, an Enter / Delete
//! pair row, and a "Back to main menu" footer.
//!
//! Pressing "Enter" submits a [`WorldAction::Enter`] into the
//! [`WorldActionQueue`]; the app drains and constructs the live `Game`. The
//! list refreshes every frame (cheap directory scan; only happens while the
//! user is on this screen) so a freshly-created world appears immediately
//! after "Create" pops back to us.

use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use egui::{Color32, Context};

use super::{
    CreateWorldScreen, caption_row, flex_spacer, footer_height, full_row_button, menu_panel,
    pair_row, t, MENU_ROW_HEIGHT, MENU_ROW_SPACING,
};
use crate::globalization::I18n;
use crate::ui::action::{WorldAction, WorldActionQueue};
use crate::ui::screen::{Screen, Transition};
use crate::worlds::World;

/// World selection screen.
pub struct WorldSelectScreen {
    worlds_root: PathBuf,
    i18n: Arc<Mutex<I18n>>,
    actions: Arc<WorldActionQueue>,
    /// Cache of world directory names. Refreshed on construction and on
    /// every frame so deletions / creations land immediately.
    entries: Vec<String>,
    /// Currently-selected world name, or empty if nothing selected. The
    /// "Enter" / "Delete" buttons act on this.
    selected: String,
}

impl WorldSelectScreen {
    #[must_use]
    pub fn new(
        worlds_root: PathBuf,
        i18n: Arc<Mutex<I18n>>,
        actions: Arc<WorldActionQueue>,
    ) -> Self {
        let entries = World::list_worlds_at(&worlds_root);
        Self {
            worlds_root,
            i18n,
            actions,
            entries,
            selected: String::new(),
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

    fn ui(&mut self, ctx: &Context) -> Transition {
        // Cheap directory scan; only while this screen is on top.
        self.refresh();

        let caption = t(&self.i18n, "NEWorld.worlds.caption");
        let new_label = t(&self.i18n, "NEWorld.worlds.new");
        let enter_label = t(&self.i18n, "NEWorld.worlds.enter");
        let delete_label = t(&self.i18n, "NEWorld.worlds.delete");
        let back_label = t(&self.i18n, "NEWorld.worlds.back");

        let mut transition = Transition::None;
        let mut create_clicked = false;
        let mut delete_clicked = false;
        let mut back_clicked = false;
        let mut enter_clicked = false;

        menu_panel(ctx, |ui| {
            caption_row(ui, &caption);
            ui.add_space(MENU_ROW_SPACING);

            // Scrollable list of world entries. Reserve enough vertical space
            // that the footer rows still fit at the bottom of the chrome.
            let list_height = (ui.available_height()
                - MENU_ROW_HEIGHT * 3.0
                - MENU_ROW_SPACING * 4.0)
                .max(MENU_ROW_HEIGHT);
            egui::ScrollArea::vertical()
                .max_height(list_height)
                .show(ui, |ui| {
                    if self.entries.is_empty() {
                        ui.add_space(MENU_ROW_HEIGHT);
                        ui.vertical_centered(|ui| {
                            ui.colored_label(
                                Color32::from_gray(180),
                                "(no worlds yet — create one below)",
                            );
                        });
                    } else {
                        let entries = self.entries.clone();
                        for name in &entries {
                            let is_selected = self.selected == *name;
                            // 72-px-tall row to match the C++ `Sizer({.max_height = 72})`.
                            let resp = ui.add_sized(
                                egui::vec2(ui.available_width(), 72.0),
                                egui::Button::selectable(is_selected, name),
                            );
                            if resp.clicked() {
                                self.selected = name.clone();
                            }
                            if resp.double_clicked() {
                                self.selected = name.clone();
                                enter_clicked = true;
                            }
                            ui.add_space(MENU_ROW_SPACING);
                        }
                    }
                });

            ui.add_space(MENU_ROW_SPACING);
            if full_row_button(ui, &new_label) {
                create_clicked = true;
            }
            ui.add_space(MENU_ROW_SPACING);

            // Footer is two rows: pair_row (32) + spacing (8) + full-row (32).
            flex_spacer(ui, footer_height(2));

            // Enter / Delete pair, then full-width Back.
            let has_sel = !self.selected.is_empty();
            pair_row(ui, |cols| {
                if cols[0]
                    .add_enabled(has_sel, egui::Button::new(&enter_label))
                    .clicked()
                {
                    enter_clicked = true;
                }
                if cols[1]
                    .add_enabled(has_sel, egui::Button::new(&delete_label))
                    .clicked()
                {
                    delete_clicked = true;
                }
            });
            ui.add_space(MENU_ROW_SPACING);
            if full_row_button(ui, &back_label) {
                back_clicked = true;
            }
        });

        if back_clicked {
            transition = Transition::Pop;
        } else if create_clicked {
            transition = Transition::Push(Box::new(CreateWorldScreen::new(
                self.worlds_root.clone(),
                Arc::clone(&self.i18n),
                Arc::clone(&self.actions),
            )));
        } else if delete_clicked {
            self.actions.submit(WorldAction::Delete {
                name: self.selected.clone(),
            });
            self.refresh();
        } else if enter_clicked && !self.selected.is_empty() {
            self.actions.submit(WorldAction::Enter {
                name: self.selected.clone(),
                seed: derive_seed(&self.selected),
            });
            transition = Transition::Pop;
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

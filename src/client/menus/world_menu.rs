//! World selection screen — direct mirror of `old/src/menus/world_menu.cpp`.
//!
//! First menu rebuilt against the in-house Flutter-style layout in
//! [`crate::ui::widgets`]. Compared to the egui-immediate-mode version this
//! replaces:
//!
//! * `available_height - 3*ROW - 4*SPACING` arithmetic for the scroll
//!   area's height with `FlexItem::flex(1.0, ScrollView::vertical(...))` —
//!   the column distributes leftover space automatically.
//! * `egui::ScrollArea::vertical().max_height(...)` with our own
//!   [`ScrollView`](crate::ui::widgets::ScrollView): it bounds the cross
//!   axis and gives the child unbounded constraint along the scroll axis,
//!   then clips + offsets in `show`.
//! * `pair_row` / `caption_row` / `full_row_button` chrome helpers with
//!   plain `Row`/`Column`/`Sizer`/`Spacer` primitives.
//!
//! Atomic widgets (entries, Enter/Delete/Back, Create-new) are still real
//! `egui::Button`s, hosted at our absolute rect via the
//! [`Button`](crate::ui::widgets::Button) /
//! [`SelectButton`](crate::ui::widgets::SelectButton) wrappers. Each takes
//! a `&mut bool` (or small output struct) and writes the extracted event
//! directly — no `egui::Response` storage, no Context-clone deadlock
//! surface, just plain bools to inspect after `run()` returns.

use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use egui::Context;

use super::action::{WorldAction, WorldActionQueue};
use super::screen::{Screen, Transition};
use super::{
    CreateWorldScreen, MENU_COL_SPACING, MENU_MAX_WIDTH, MENU_PADDING, MENU_ROW_HEIGHT,
    MENU_ROW_SPACING, t,
};
use crate::globalization::I18n;
use crate::ui;
use crate::ui::widgets::{
    Aligned, Alignment, Button, CrossAxisSize, Flex, FlexItem, Label, MainAxisSize, Padding,
    ScrollView, SelectButton, Sizer, Spacer,
};
use crate::core::world::list_worlds_at;

/// Height of a single world-list entry, in logical pixels — matches the
/// C++ `Sizer({.max_height = 72})` from `old/src/menus/world_menu.cpp:46`.
const ENTRY_ROW_HEIGHT: f32 = 72.0;

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
    
    pub fn new(
        worlds_root: PathBuf,
        i18n: Arc<Mutex<I18n>>,
        actions: Arc<WorldActionQueue>,
    ) -> Self {
        let entries = list_worlds_at(&worlds_root);
        Self {
            worlds_root,
            i18n,
            actions,
            entries,
            selected: String::new(),
        }
    }

    fn refresh(&mut self) {
        self.entries = list_worlds_at(&self.worlds_root);
        if !self.entries.iter().any(|n| n == &self.selected) {
            self.selected.clear();
        }
    }
}

impl Screen for WorldSelectScreen {
    fn title(&self) -> &'static str {
        "Select World"
    }

    fn show(&mut self, ctx: &Context) -> Transition {
        // Cheap directory scan; only while this screen is on top.
        self.refresh();

        let caption = t(&self.i18n, "NEWorld.worlds.caption");
        let new_label = t(&self.i18n, "NEWorld.worlds.new");
        let enter_label = t(&self.i18n, "NEWorld.worlds.enter");
        let delete_label = t(&self.i18n, "NEWorld.worlds.delete");
        let back_label = t(&self.i18n, "NEWorld.worlds.back");

        // Output slots — every interactive widget below borrows one of
        // these mutably for the duration of the build. Inspected after
        // `run()` returns, when all widget borrows have dropped.
        let mut create_clicked = false;
        let mut enter_clicked = false;
        let mut delete_clicked = false;
        let mut back_clicked = false;
        // Per-entry click + double-click slots — parallel vecs so each
        // `SelectButton` can borrow its own pair of `&mut bool`s.
        let mut entry_clicked: Vec<bool> = vec![false; self.entries.len()];
        let mut entry_double_clicked: Vec<bool> = vec![false; self.entries.len()];

        let has_sel = !self.selected.is_empty();
        let selected_name = self.selected.clone();

        // ---- Build the entry list (vertical column, scrolled) ----
        let mut entry_items: Vec<FlexItem> = Vec::new();
        let entry_slots = entry_clicked
            .iter_mut()
            .zip(entry_double_clicked.iter_mut());
        for (name, (clicked_slot, double_slot)) in self.entries.iter().zip(entry_slots) {
            let is_sel = name == &selected_name;
            entry_items.push(FlexItem::new(Sizer::height(
                ENTRY_ROW_HEIGHT,
                SelectButton::new(name, is_sel)
                    .clicked(clicked_slot)
                    .double_clicked(double_slot),
            )));
            entry_items.push(FlexItem::new(Spacer::height(MENU_ROW_SPACING)));
        }
        entry_items.push(FlexItem::new(Sizer::height(
            ENTRY_ROW_HEIGHT,
            Button::new(&new_label).clicked(&mut create_clicked),
        )));
        let entries_column = Flex::column(entry_items);

        // ---- Compose the body column ----
        let body = Flex::column(vec![
            // caption
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Aligned::center(Label::new(&caption)),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // scrollable entries — flex-grow consumes leftover height
            FlexItem::flex(
                1.0,
                ScrollView::vertical(egui::Id::new("worlds.scroll"), entries_column),
            ),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Enter | Delete pair
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(
                        1.0,
                        Button::new(&enter_label)
                            .clicked(&mut enter_clicked)
                            .enabled(has_sel),
                    ),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(
                        1.0,
                        Button::new(&delete_label)
                            .clicked(&mut delete_clicked)
                            .enabled(has_sel),
                    ),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Back to main menu
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Button::new(&back_label).clicked(&mut back_clicked),
            )),
        ])
        .main_size(MainAxisSize::Max)
        .cross_size(CrossAxisSize::Max);

        // Outer chrome: padding + max-width + horizontal centering.
        // No boxing at the root — `run()` takes any `E: Element` by value
        // and monomorphises per call site.
        let root = Aligned::new(
            Alignment::TopCenter,
            Padding::all(MENU_PADDING, Sizer::width(MENU_MAX_WIDTH, body)),
        );

        // ---- Run one frame ----
        ui::show(ctx, root);

        // ---- Drain output slots into screen state / transitions ----
        let mut want_enter = enter_clicked;
        for (i, (&clicked, &double_clicked)) in entry_clicked
            .iter()
            .zip(entry_double_clicked.iter())
            .enumerate()
        {
            if clicked {
                self.selected = self.entries[i].clone();
            }
            if double_clicked {
                self.selected = self.entries[i].clone();
                want_enter = true;
            }
        }

        if back_clicked {
            Transition::Pop
        } else if create_clicked {
            Transition::Push(Box::new(CreateWorldScreen::new(
                self.worlds_root.clone(),
                Arc::clone(&self.i18n),
                Arc::clone(&self.actions),
            )))
        } else if delete_clicked && !self.selected.is_empty() {
            self.actions.submit(WorldAction::Delete {
                name: self.selected.clone(),
            });
            self.refresh();
            Transition::None
        } else if want_enter && !self.selected.is_empty() {
            self.actions.submit(WorldAction::Enter {
                name: self.selected.clone(),
                seed: derive_seed(&self.selected),
            });
            Transition::Pop
        } else {
            Transition::None
        }
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

//! Language picker — direct mirror of `old/src/menus/language_menu.cpp`.
//!
//! Caption row, one full-row button per available language (label = native
//! name), then a Back button.
//!
//! On click the picker writes `lang_code` into `Config::language`; the App's
//! per-frame `apply_config` sees the change and reloads the i18n table so
//! every menu drawn from the next frame onward uses the new strings.
//!
//! The list is built by scanning `<assets>/lang/*.toml` via
//! [`crate::globalization::list_languages`], matching the C++ build's
//! `lang/langs.txt` index walk.

use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use egui::Context;

use super::screen::{Screen, Transition};
use super::{MENU_MAX_WIDTH, MENU_PADDING, MENU_ROW_HEIGHT, MENU_ROW_SPACING, t};
use crate::config::Config;
use crate::globalization::{I18n, LanguageEntry, list_languages};
use crate::ui;
use crate::ui::widgets::{
    Aligned, Alignment, Button, Flex, FlexItem, Label, MainAxisAlignment, MainAxisSize, Padding,
    ScrollView, Sizer, Spacer,
};

/// Language picker sub-screen.
pub struct LanguageScreen {
    config: Arc<Mutex<Config>>,
    i18n: Arc<Mutex<I18n>>,
    /// Cached list of available languages. Refreshed at construction;
    /// adding a new language while the menu is open is uncommon enough that
    /// rescanning per-frame would just be wasted I/O.
    entries: Vec<LanguageEntry>,
}

impl LanguageScreen {
    pub fn new(config: Arc<Mutex<Config>>, i18n: Arc<Mutex<I18n>>) -> Self {
        let entries = list_languages(&lang_dir());
        Self {
            config,
            i18n,
            entries,
        }
    }
}

impl Screen for LanguageScreen {
    fn title(&self) -> &'static str {
        "Language"
    }

    fn show(&mut self, ctx: &Context) -> Transition {
        let caption = t(&self.i18n, "NEWorld.language.caption");
        let back_lbl = t(&self.i18n, "NEWorld.language.back");

        let mut entry_clicked: Vec<bool> = vec![false; self.entries.len()];
        let mut back_clicked = false;

        let mut entry_items: Vec<FlexItem> = Vec::new();
        for (entry, slot) in self.entries.iter().zip(entry_clicked.iter_mut()) {
            let label = if entry.native_name.is_empty() {
                &entry.code
            } else {
                &entry.native_name
            };
            entry_items.push(FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Button::new(label).clicked(slot),
            )));
            entry_items.push(FlexItem::new(Spacer::height(MENU_ROW_SPACING)));
        }
        let entries_column = Flex::column(entry_items);

        let body = Flex::column(vec![
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Aligned::center(Label::new(&caption)),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            FlexItem::flex(
                1.0,
                ScrollView::vertical(egui::Id::new("language.scroll"), entries_column),
            ),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Button::new(&back_lbl).clicked(&mut back_clicked),
            )),
        ])
        .main_size(MainAxisSize::Max)
        .main_align(MainAxisAlignment::Start);

        let root = Aligned::new(
            Alignment::TopCenter,
            Padding::all(MENU_PADDING, Sizer::width(MENU_MAX_WIDTH, body)),
        );

        ui::show(ctx, root);

        // Drain entry clicks first — a click writes config + pops.
        for (i, &clicked) in entry_clicked.iter().enumerate() {
            if clicked {
                if let Ok(mut cfg) = self.config.lock() {
                    cfg.language = self.entries[i].code.clone();
                }
                return Transition::Pop;
            }
        }
        if back_clicked {
            Transition::Pop
        } else {
            Transition::None
        }
    }
}

/// Resolve `<assets>/lang/`. Mirrors the path resolution in `App::resumed`
/// without depending on App-private helpers.
fn lang_dir() -> PathBuf {
    if let Some(dir) = option_env!("CARGO_MANIFEST_DIR") {
        return PathBuf::from(dir).join("assets").join("lang");
    }
    PathBuf::from("assets").join("lang")
}

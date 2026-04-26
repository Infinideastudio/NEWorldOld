//! Language picker — direct mirror of `old/src/menus/language_menu.cpp`.
//!
//! Layout (top-down):
//!   * caption row
//!   * one full-row button per available language (label = native name)
//!   * flex spacer
//!   * back
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

use super::{
    caption_row, flex_spacer, footer_height, full_row_button, menu_panel, t, MENU_ROW_SPACING,
};
use crate::config::Config;
use crate::globalization::{I18n, LanguageEntry, list_languages};
use crate::ui::screen::{Screen, Transition};

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
    #[must_use]
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

    fn ui(&mut self, ctx: &Context) -> Transition {
        let caption = t(&self.i18n, "NEWorld.language.caption");
        let back_lbl = t(&self.i18n, "NEWorld.language.back");

        let mut transition = Transition::None;
        let mut chosen_code: Option<String> = None;
        let mut want_back = false;

        menu_panel(ctx, |ui| {
            caption_row(ui, &caption);
            ui.add_space(MENU_ROW_SPACING);

            for entry in &self.entries {
                let label = if entry.native_name.is_empty() {
                    entry.code.clone()
                } else {
                    entry.native_name.clone()
                };
                if full_row_button(ui, &label) {
                    chosen_code = Some(entry.code.clone());
                }
                ui.add_space(MENU_ROW_SPACING);
            }

            // Footer: one full-width back button (32 px).
            flex_spacer(ui, footer_height(1));

            if full_row_button(ui, &back_lbl) {
                want_back = true;
            }
        });

        if let Some(code) = chosen_code {
            // Update the config; App::apply_config picks up the change next
            // frame and reloads the i18n table.
            if let Ok(mut cfg) = self.config.lock() {
                cfg.language = code;
            }
            transition = Transition::Pop;
        } else if want_back {
            transition = Transition::Pop;
        }

        transition
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

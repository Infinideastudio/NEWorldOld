//! UI options screen — direct mirror of `old/src/menus/ui_options_menu.cpp`.
//!
//! Layout (top-down):
//!   * caption row
//!   * font size slider | "PPI stretch" toggle
//!   * "background blur" toggle | (empty filler — matches the C++ which also
//!     leaves the right column of the second row blank)
//!   * flex spacer
//!   * back
//!
//! Font size is live (App pushes the value into egui's pixel scale every
//! frame). PPI stretch and background blur persist into `Config` but don't
//! yet affect rendering — see `docs/rust_migration.md` Tier 3.

use std::sync::{Arc, Mutex};

use egui::Context;

use super::{
    caption_row, flex_spacer, footer_height, full_row_button, menu_panel, pair_row, t,
    MENU_ROW_SPACING,
};
use crate::config::Config;
use crate::globalization::I18n;
use crate::ui::screen::{Screen, Transition};

/// UI options sub-screen.
pub struct UIOptionsScreen {
    config: Arc<Mutex<Config>>,
    i18n: Arc<Mutex<I18n>>,
}

impl UIOptionsScreen {
    #[must_use]
    pub fn new(config: Arc<Mutex<Config>>, i18n: Arc<Mutex<I18n>>) -> Self {
        Self { config, i18n }
    }
}

impl Screen for UIOptionsScreen {
    fn title(&self) -> &'static str {
        "UI Options"
    }

    fn ui(&mut self, ctx: &Context) -> Transition {
        let caption = t(&self.i18n, "NEWorld.gui.caption");
        let font_lbl = t(&self.i18n, "NEWorld.gui.fontsize");
        let stretch_lbl = t(&self.i18n, "NEWorld.gui.stretch");
        let blur_lbl = t(&self.i18n, "NEWorld.gui.blur");
        let back_lbl = t(&self.i18n, "NEWorld.gui.back");
        let enabled_lbl = t(&self.i18n, "NEWorld.enabled");
        let disabled_lbl = t(&self.i18n, "NEWorld.disabled");

        let mut transition = Transition::None;
        let mut want_back = false;

        let mut cfg = self.config.lock().expect("config poisoned");

        menu_panel(ctx, |ui| {
            caption_row(ui, &caption);
            ui.add_space(MENU_ROW_SPACING);

            // Row 1: font size slider | PPI stretch toggle
            pair_row(ui, |cols| {
                cols[0].label(format!("{font_lbl}{:.1}x", cfg.font_scale));
                cols[0].add(egui::Slider::new(&mut cfg.font_scale, 0.5..=2.0).show_value(false));
                let stretch = format!(
                    "{stretch_lbl}{}",
                    if cfg.ui_auto_stretch { &enabled_lbl } else { &disabled_lbl }
                );
                if cols[1].button(stretch).clicked() {
                    cfg.ui_auto_stretch = !cfg.ui_auto_stretch;
                }
            });
            ui.add_space(MENU_ROW_SPACING);

            // Row 2: background blur toggle | empty (matches C++ layout)
            pair_row(ui, |cols| {
                let blur = format!(
                    "{blur_lbl}{}",
                    if cfg.ui_background_blur { &enabled_lbl } else { &disabled_lbl }
                );
                if cols[0].button(blur).clicked() {
                    cfg.ui_background_blur = !cfg.ui_background_blur;
                }
                // cols[1] left blank — matches the C++ layout.
            });
            ui.add_space(MENU_ROW_SPACING);

            // Footer: one full-width back button (32 px).
            flex_spacer(ui, footer_height(1));

            if full_row_button(ui, &back_lbl) {
                want_back = true;
            }

            cfg.font_scale = cfg.font_scale.clamp(0.5, 2.0);
        });

        drop(cfg);

        if want_back {
            transition = Transition::Pop;
        }
        transition
    }
}

//! Render options screen — direct mirror of
//! `old/src/menus/render_options_menu.cpp`.
//!
//! Layout (top-down):
//!   * caption row
//!   * smooth lighting toggle | fancy grass toggle
//!   * merge face toggle | MSAA slider
//!   * vsync toggle | "advanced rendering" sub-menu button
//!   * flex spacer
//!   * back
//!
//! Most of these settings persist into `Config` but don't yet affect the
//! Rust renderer (smooth lighting, fancy grass, merge-face, MSAA — see the
//! roadmap in `docs/rust_migration.md` §4 Tier 2/3). The toggles are still
//! exposed so the layout matches C++ verbatim and the values survive the
//! day a renderer feature lands.

use std::sync::{Arc, Mutex};

use egui::Context;

use super::{
    ShaderOptionsScreen, caption_row, flex_spacer, footer_height, full_row_button, menu_panel,
    pair_row, t, MENU_ROW_SPACING,
};
use crate::config::Config;
use crate::globalization::I18n;
use crate::ui::screen::{Screen, Transition};

/// Render options sub-screen.
pub struct RenderOptionsScreen {
    config: Arc<Mutex<Config>>,
    i18n: Arc<Mutex<I18n>>,
}

impl RenderOptionsScreen {
    #[must_use]
    pub fn new(config: Arc<Mutex<Config>>, i18n: Arc<Mutex<I18n>>) -> Self {
        Self { config, i18n }
    }
}

impl Screen for RenderOptionsScreen {
    fn title(&self) -> &'static str {
        "Render Options"
    }

    fn ui(&mut self, ctx: &Context) -> Transition {
        let caption = t(&self.i18n, "NEWorld.render.caption");
        let smooth_lbl = t(&self.i18n, "NEWorld.render.smooth");
        let grass_lbl = t(&self.i18n, "NEWorld.render.grasstex");
        let merge_lbl = t(&self.i18n, "NEWorld.render.merge");
        let msaa_lbl = t(&self.i18n, "NEWorld.render.multisample");
        let vsync_lbl = t(&self.i18n, "NEWorld.render.vsync");
        let shaders_lbl = t(&self.i18n, "NEWorld.render.shaders");
        let back_lbl = t(&self.i18n, "NEWorld.render.back");
        let enabled_lbl = t(&self.i18n, "NEWorld.enabled");
        let disabled_lbl = t(&self.i18n, "NEWorld.disabled");

        let mut transition = Transition::None;
        let mut want_back = false;
        let mut want_shaders = false;

        let mut cfg = self.config.lock().expect("config poisoned");

        menu_panel(ctx, |ui| {
            caption_row(ui, &caption);
            ui.add_space(MENU_ROW_SPACING);

            // Row 1: smooth lighting | fancy grass
            pair_row(ui, |cols| {
                let smooth = format!(
                    "{smooth_lbl}{}",
                    bool_state(cfg.smooth_lighting, &enabled_lbl, &disabled_lbl)
                );
                if cols[0].button(smooth).clicked() {
                    cfg.smooth_lighting = !cfg.smooth_lighting;
                }
                let grass = format!(
                    "{grass_lbl}{}",
                    bool_state(cfg.nice_grass, &enabled_lbl, &disabled_lbl)
                );
                if cols[1].button(grass).clicked() {
                    cfg.nice_grass = !cfg.nice_grass;
                }
            });
            ui.add_space(MENU_ROW_SPACING);

            // Row 2: merge face | MSAA slider
            pair_row(ui, |cols| {
                let merge = format!(
                    "{merge_lbl}{}",
                    bool_state(cfg.merge_face, &enabled_lbl, &disabled_lbl)
                );
                if cols[0].button(merge).clicked() {
                    cfg.merge_face = !cfg.merge_face;
                }
                let value_text = if cfg.multisample == 0 {
                    disabled_lbl.clone()
                } else {
                    format!("{}x", cfg.multisample)
                };
                cols[1].label(format!("{msaa_lbl}{value_text}"));
                // C++: log2 levels [0, 2, 4, 8] mapped to slider position [0, 0.33, 0.66, 1].
                let mut pos = msaa_to_position(cfg.multisample);
                if cols[1]
                    .add(egui::Slider::new(&mut pos, 0.0..=1.0).show_value(false))
                    .changed()
                {
                    cfg.multisample = position_to_msaa(pos);
                }
            });
            ui.add_space(MENU_ROW_SPACING);

            // Row 3: vsync | advanced rendering sub-menu
            pair_row(ui, |cols| {
                let vsync = format!(
                    "{vsync_lbl}{}",
                    bool_state(cfg.vertical_sync, &enabled_lbl, &disabled_lbl)
                );
                if cols[0].button(vsync).clicked() {
                    cfg.vertical_sync = !cfg.vertical_sync;
                }
                if cols[1].button(&shaders_lbl).clicked() {
                    want_shaders = true;
                }
            });
            ui.add_space(MENU_ROW_SPACING);

            // Footer: one full-width back button (32 px).
            flex_spacer(ui, footer_height(1));

            if full_row_button(ui, &back_lbl) {
                want_back = true;
            }
        });

        drop(cfg);

        if want_back {
            transition = Transition::Pop;
        } else if want_shaders {
            transition = Transition::Push(Box::new(ShaderOptionsScreen::new(
                Arc::clone(&self.config),
                Arc::clone(&self.i18n),
            )));
        }

        transition
    }
}

fn bool_state(value: bool, enabled: &str, disabled: &str) -> String {
    if value {
        enabled.to_owned()
    } else {
        disabled.to_owned()
    }
}

/// C++ `_msaa_to_position`: `level <= 1 ? 0 : log2(level) / 3`.
fn msaa_to_position(level: i32) -> f32 {
    if level <= 1 {
        0.0
    } else {
        (level as f32).log2() / 3.0
    }
}

/// C++ `_position_to_msaa`: rounds slider position to {0, 2, 4, 8}.
fn position_to_msaa(position: f32) -> i32 {
    let level = 2_f32.powf(position.mul_add(3.0, 0.0).round()) as i32;
    if level <= 1 { 0 } else { level }
}

//! Shader options screen — direct mirror of
//! `old/src/menus/shader_options_menu.cpp`.
//!
//! Layout (top-down):
//!   * caption row
//!   * "advanced rendering enabled" toggle | shadow resolution slider
//!   * shadow distance slider | soft shadow toggle
//!   * volumetric clouds toggle | ambient occlusion toggle
//!   * flex spacer
//!   * back
//!
//! The Rust renderer doesn't yet implement shadows / clouds / SSR, so these
//! toggles persist into `Config` and become live when the renderer feature
//! lands (`docs/rust_migration.md` §4 Tier 4). The layout matches C++
//! verbatim so the menu doesn't have to be rebuilt later.

use std::sync::{Arc, Mutex};

use egui::Context;

use super::{MENU_ROW_SPACING, caption_row, full_row_button, menu_panel, pair_row, t};
use crate::config::Config;
use crate::globalization::I18n;
use crate::ui::screen::{Screen, Transition};

/// Shader / advanced-rendering sub-screen.
pub struct ShaderOptionsScreen {
    config: Arc<Mutex<Config>>,
    i18n: Arc<Mutex<I18n>>,
}

impl ShaderOptionsScreen {
    #[must_use]
    pub fn new(config: Arc<Mutex<Config>>, i18n: Arc<Mutex<I18n>>) -> Self {
        Self { config, i18n }
    }
}

impl Screen for ShaderOptionsScreen {
    fn title(&self) -> &'static str {
        "Shader Options"
    }

    fn ui(&mut self, ctx: &Context) -> Transition {
        let caption = t(&self.i18n, "NEWorld.shaders.caption");
        let enable_lbl = t(&self.i18n, "NEWorld.shaders.enable");
        let res_lbl = t(&self.i18n, "NEWorld.shaders.shadowres");
        let dist_lbl = t(&self.i18n, "NEWorld.shaders.distance");
        let soft_lbl = t(&self.i18n, "NEWorld.shaders.softshadow");
        let clouds_lbl = t(&self.i18n, "NEWorld.shaders.clouds");
        let ssao_lbl = t(&self.i18n, "NEWorld.shaders.ssao");
        let back_lbl = t(&self.i18n, "NEWorld.shaders.back");
        let yes_lbl = t(&self.i18n, "NEWorld.yes");
        let no_lbl = t(&self.i18n, "NEWorld.no");
        let enabled_lbl = t(&self.i18n, "NEWorld.enabled");
        let disabled_lbl = t(&self.i18n, "NEWorld.disabled");

        let mut transition = Transition::None;
        let mut want_back = false;

        let mut cfg = self.config.lock().expect("config poisoned");

        menu_panel(ctx, "shader_options", |ui| {
            caption_row(ui, &caption);
            ui.add_space(MENU_ROW_SPACING);

            // Row 1: advanced rendering enabled (yes/no) | shadow resolution slider
            pair_row(ui, |cols| {
                let enable = format!(
                    "{enable_lbl}{}",
                    yes_no(cfg.advanced_render, &yes_lbl, &no_lbl)
                );
                if cols[0].button(enable).clicked() {
                    cfg.advanced_render = !cfg.advanced_render;
                }
                cols[1].label(format!("{res_lbl}{}x", cfg.shadow_res));
                let mut pos = shadow_res_to_position(cfg.shadow_res);
                if cols[1]
                    .add(egui::Slider::new(&mut pos, 0.0..=1.0).show_value(false))
                    .changed()
                {
                    cfg.shadow_res = position_to_shadow_res(pos);
                }
            });
            ui.add_space(MENU_ROW_SPACING);

            // Row 2: shadow distance slider | soft shadow toggle
            pair_row(ui, |cols| {
                cols[0].label(format!("{dist_lbl}{}", cfg.max_shadow_distance));
                cols[0].add(egui::Slider::new(&mut cfg.max_shadow_distance, 4..=32).show_value(false));
                let soft = format!(
                    "{soft_lbl}{}",
                    enabled_disabled(cfg.soft_shadow, &enabled_lbl, &disabled_lbl)
                );
                if cols[1].button(soft).clicked() {
                    cfg.soft_shadow = !cfg.soft_shadow;
                }
            });
            ui.add_space(MENU_ROW_SPACING);

            // Row 3: clouds | SSAO
            pair_row(ui, |cols| {
                let clouds = format!(
                    "{clouds_lbl}{}",
                    enabled_disabled(cfg.volumetric_clouds, &enabled_lbl, &disabled_lbl)
                );
                if cols[0].button(clouds).clicked() {
                    cfg.volumetric_clouds = !cfg.volumetric_clouds;
                }
                let ssao = format!(
                    "{ssao_lbl}{}",
                    enabled_disabled(cfg.ambient_occlusion, &enabled_lbl, &disabled_lbl)
                );
                if cols[1].button(ssao).clicked() {
                    cfg.ambient_occlusion = !cfg.ambient_occlusion;
                }
            });
            ui.add_space(MENU_ROW_SPACING);

            // Footer back button — natural bottom of the centred body.
            if full_row_button(ui, &back_lbl) {
                want_back = true;
            }
        });

        drop(cfg);

        if want_back {
            transition = Transition::Pop;
        }
        transition
    }
}

fn yes_no(value: bool, yes: &str, no: &str) -> String {
    if value { yes.to_owned() } else { no.to_owned() }
}

fn enabled_disabled(value: bool, enabled: &str, disabled: &str) -> String {
    if value { enabled.to_owned() } else { disabled.to_owned() }
}

/// C++ `_shadow_resolution_to_position`: `(log2(res) - 10) / 3`.
fn shadow_res_to_position(res: i32) -> f32 {
    ((res as f32).log2() - 10.0) / 3.0
}

/// C++ `_position_to_shadow_resolution`: round to {1024, 2048, 4096, 8192}.
fn position_to_shadow_res(position: f32) -> i32 {
    2_f32.powf((position * 3.0).round() + 10.0) as i32
}

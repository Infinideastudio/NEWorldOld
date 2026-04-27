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

use std::sync::{Arc, Mutex};

use egui::Context;

use super::screen::{Screen, Transition};
use super::{MENU_COL_SPACING, MENU_MAX_WIDTH, MENU_PADDING, MENU_ROW_HEIGHT, MENU_ROW_SPACING, t};
use crate::config::Config;
use crate::globalization::I18n;
use crate::ui;
use crate::ui::widgets::{
    Aligned, Alignment, Button, CrossAxisSize, Flex, FlexItem, Label, MainAxisAlignment,
    MainAxisSize, Padding, Sizer, Slider, Spacer,
};

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

    fn show(&mut self, ctx: &Context) -> Transition {
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

        let mut want_back = false;
        let mut enable_clicked = false;
        let mut soft_clicked = false;
        let mut clouds_clicked = false;
        let mut ssao_clicked = false;
        let mut shadow_res_changed = false;
        let mut shadow_dist_changed = false;

        let mut guard = self.config.lock().expect("config poisoned");
        let cfg: &mut Config = &mut guard;

        let enable_text = format!(
            "{enable_lbl}{}",
            yes_no(cfg.advanced_render, &yes_lbl, &no_lbl)
        );
        let res_text = format!("{res_lbl}{}x", cfg.shadow_res);
        let dist_text = format!("{dist_lbl}{}", cfg.max_shadow_distance);
        let soft_text = format!(
            "{soft_lbl}{}",
            enabled_disabled(cfg.soft_shadow, &enabled_lbl, &disabled_lbl)
        );
        let clouds_text = format!(
            "{clouds_lbl}{}",
            enabled_disabled(cfg.volumetric_clouds, &enabled_lbl, &disabled_lbl)
        );
        let ssao_text = format!(
            "{ssao_lbl}{}",
            enabled_disabled(cfg.ambient_occlusion, &enabled_lbl, &disabled_lbl)
        );

        let mut shadow_res_pos = shadow_res_to_position(cfg.shadow_res);

        let body = Flex::column(vec![
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Aligned::center(Label::new(caption)),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Row 1: advanced render toggle | shadow resolution slider
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(1.0, Button::new(enable_text, &mut enable_clicked)),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(
                        1.0,
                        Flex::column(vec![
                            FlexItem::new(Label::new(res_text)),
                            FlexItem::flex(
                                1.0,
                                Slider::new(
                                    &mut shadow_res_pos,
                                    0.0..=1.0,
                                    &mut shadow_res_changed,
                                ),
                            ),
                        ])
                        .cross_size(CrossAxisSize::Max),
                    ),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Row 2: shadow distance slider | soft shadow toggle
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(
                        1.0,
                        Flex::column(vec![
                            FlexItem::new(Label::new(dist_text)),
                            FlexItem::flex(
                                1.0,
                                Slider::new(
                                    &mut cfg.max_shadow_distance,
                                    4..=32,
                                    &mut shadow_dist_changed,
                                ),
                            ),
                        ])
                        .cross_size(CrossAxisSize::Max),
                    ),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(1.0, Button::new(soft_text, &mut soft_clicked)),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Row 3: clouds | SSAO
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(1.0, Button::new(clouds_text, &mut clouds_clicked)),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(1.0, Button::new(ssao_text, &mut ssao_clicked)),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            )),
            FlexItem::flex(1.0, Spacer::fill()),
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Button::new(back_lbl, &mut want_back),
            )),
        ])
        .main_size(MainAxisSize::Max)
        .main_align(MainAxisAlignment::Start)
        .cross_size(CrossAxisSize::Max);

        let root = Aligned::new(
            Alignment::TopCenter,
            Padding::all(MENU_PADDING, Sizer::width(MENU_MAX_WIDTH, body)),
        );

        ui::show(ctx, root);

        if enable_clicked {
            cfg.advanced_render = !cfg.advanced_render;
        }
        if soft_clicked {
            cfg.soft_shadow = !cfg.soft_shadow;
        }
        if clouds_clicked {
            cfg.volumetric_clouds = !cfg.volumetric_clouds;
        }
        if ssao_clicked {
            cfg.ambient_occlusion = !cfg.ambient_occlusion;
        }
        if shadow_res_changed {
            cfg.shadow_res = position_to_shadow_res(shadow_res_pos);
        }

        drop(guard);

        if want_back {
            Transition::Pop
        } else {
            Transition::None
        }
    }
}

fn yes_no(value: bool, yes: &str, no: &str) -> String {
    if value { yes.to_owned() } else { no.to_owned() }
}

fn enabled_disabled(value: bool, enabled: &str, disabled: &str) -> String {
    if value {
        enabled.to_owned()
    } else {
        disabled.to_owned()
    }
}

/// C++ `_shadow_resolution_to_position`: `(log2(res) - 10) / 3`.
fn shadow_res_to_position(res: i32) -> f32 {
    ((res as f32).log2() - 10.0) / 3.0
}

/// C++ `_position_to_shadow_resolution`: round to {1024, 2048, 4096, 8192}.
fn position_to_shadow_res(position: f32) -> i32 {
    2_f32.powf((position * 3.0).round() + 10.0) as i32
}

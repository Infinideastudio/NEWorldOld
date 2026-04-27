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
//! Rust renderer — the toggles are exposed so the layout matches C++
//! verbatim and the values survive the day a renderer feature lands.

use std::sync::{Arc, Mutex};

use egui::Context;

use super::screen::{Screen, Transition};
use super::{
    MENU_COL_SPACING, MENU_MAX_WIDTH, MENU_PADDING, MENU_ROW_HEIGHT, MENU_ROW_SPACING,
    ShaderOptionsScreen, t,
};
use crate::config::Config;
use crate::globalization::I18n;
use crate::ui;
use crate::ui::widgets::{
    Aligned, Alignment, Button, CrossAxisSize, Flex, FlexItem, Label, MainAxisAlignment,
    MainAxisSize, Padding, Sizer, Slider, Spacer,
};

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

    fn show(&mut self, ctx: &Context) -> Transition {
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

        let mut want_back = false;
        let mut want_shaders = false;
        let mut smooth_clicked = false;
        let mut grass_clicked = false;
        let mut merge_clicked = false;
        let mut vsync_clicked = false;
        let mut msaa_changed = false;

        let mut guard = self.config.lock().expect("config poisoned");
        let cfg: &mut Config = &mut guard;

        // Snapshot label texts for the toggles.
        let smooth_text = format!(
            "{smooth_lbl}{}",
            bool_state(cfg.smooth_lighting, &enabled_lbl, &disabled_lbl)
        );
        let grass_text = format!(
            "{grass_lbl}{}",
            bool_state(cfg.nice_grass, &enabled_lbl, &disabled_lbl)
        );
        let merge_text = format!(
            "{merge_lbl}{}",
            bool_state(cfg.merge_face, &enabled_lbl, &disabled_lbl)
        );
        let vsync_text = format!(
            "{vsync_lbl}{}",
            bool_state(cfg.vertical_sync, &enabled_lbl, &disabled_lbl)
        );
        let msaa_value_text = if cfg.multisample == 0 {
            disabled_lbl.clone()
        } else {
            format!("{}x", cfg.multisample)
        };
        let msaa_text = format!("{msaa_lbl}{msaa_value_text}");
        // C++: log2 levels [0, 2, 4, 8] mapped to slider position [0, 0.33, 0.66, 1].
        let mut msaa_pos = msaa_to_position(cfg.multisample);

        let body = Flex::column(vec![
            // Caption.
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Aligned::center(Label::new(caption)),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Row 1: smooth lighting | fancy grass
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(1.0, Button::new(smooth_text, &mut smooth_clicked)),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(1.0, Button::new(grass_text, &mut grass_clicked)),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Row 2: merge face | MSAA slider
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(1.0, Button::new(merge_text, &mut merge_clicked)),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(
                        1.0,
                        Flex::column(vec![
                            FlexItem::new(Label::new(msaa_text)),
                            FlexItem::flex(
                                1.0,
                                Slider::new(&mut msaa_pos, 0.0..=1.0, &mut msaa_changed),
                            ),
                        ])
                        .cross_size(CrossAxisSize::Max),
                    ),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Row 3: vsync | shader sub-menu
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(1.0, Button::new(vsync_text, &mut vsync_clicked)),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(1.0, Button::new(shaders_lbl, &mut want_shaders)),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            )),
            // Flex spacer pushes the back row to the bottom.
            FlexItem::flex(1.0, Spacer::fill()),
            // Footer: full-width Back.
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

        if smooth_clicked {
            cfg.smooth_lighting = !cfg.smooth_lighting;
        }
        if grass_clicked {
            cfg.nice_grass = !cfg.nice_grass;
        }
        if merge_clicked {
            cfg.merge_face = !cfg.merge_face;
        }
        if vsync_clicked {
            cfg.vertical_sync = !cfg.vertical_sync;
        }
        if msaa_changed {
            cfg.multisample = position_to_msaa(msaa_pos);
        }

        drop(guard);

        if want_back {
            Transition::Pop
        } else if want_shaders {
            Transition::Push(Box::new(ShaderOptionsScreen::new(
                Arc::clone(&self.config),
                Arc::clone(&self.i18n),
            )))
        } else {
            Transition::None
        }
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

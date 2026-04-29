//! Render options screen — direct mirror of
//! `old/src/menus/render_options_menu.cpp`.
//!
//! Layout (top-down):
//!   * caption row
//!   * smooth lighting toggle | fancy grass toggle
//!   * merge face toggle | vsync toggle
//!   * "advanced rendering" sub-menu button | (filler)
//!   * flex spacer
//!   * back

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
    MainAxisSize, Padding, Sizer, Spacer,
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
            // Row 2: merge face | vsync
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(1.0, Button::new(merge_text, &mut merge_clicked)),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(1.0, Button::new(vsync_text, &mut vsync_clicked)),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Row 3: shader sub-menu | (filler)
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(1.0, Button::new(shaders_lbl, &mut want_shaders)),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(1.0, Spacer::fill()),
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

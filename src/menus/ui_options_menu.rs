//! UI options screen — direct mirror of `old/src/menus/ui_options_menu.cpp`.
//!
//! Layout (top-down):
//!   * caption row
//!   * font size slider | "PPI stretch" toggle
//!   * "background blur" toggle | (empty filler — matches the C++ which also
//!     leaves the right column of the second row blank)
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

    fn show(&mut self, ctx: &Context) -> Transition {
        let caption = t(&self.i18n, "NEWorld.gui.caption");
        let font_lbl = t(&self.i18n, "NEWorld.gui.fontsize");
        let stretch_lbl = t(&self.i18n, "NEWorld.gui.stretch");
        let blur_lbl = t(&self.i18n, "NEWorld.gui.blur");
        let back_lbl = t(&self.i18n, "NEWorld.gui.back");
        let enabled_lbl = t(&self.i18n, "NEWorld.enabled");
        let disabled_lbl = t(&self.i18n, "NEWorld.disabled");

        let mut want_back = false;
        let mut stretch_clicked = false;
        let mut blur_clicked = false;
        let mut font_changed = false;

        let mut guard = self.config.lock().expect("config poisoned");
        let cfg: &mut Config = &mut guard;

        let font_text = format!("{font_lbl}{:.1}x", cfg.font_scale);
        let stretch_text = format!(
            "{stretch_lbl}{}",
            if cfg.ui_auto_stretch {
                &enabled_lbl
            } else {
                &disabled_lbl
            }
        );
        let blur_text = format!(
            "{blur_lbl}{}",
            if cfg.ui_background_blur {
                &enabled_lbl
            } else {
                &disabled_lbl
            }
        );

        let body = Flex::column(vec![
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Aligned::center(Label::new(caption)),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Row 1: font size slider | stretch toggle
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(
                        1.0,
                        Flex::column(vec![
                            FlexItem::new(Label::new(font_text)),
                            FlexItem::flex(
                                1.0,
                                Slider::new(&mut cfg.font_scale, 0.5..=2.0, &mut font_changed),
                            ),
                        ])
                        .cross_size(CrossAxisSize::Max),
                    ),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(1.0, Button::new(stretch_text, &mut stretch_clicked)),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Row 2: blur toggle | (left blank to match C++)
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(1.0, Button::new(blur_text, &mut blur_clicked)),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(1.0, Spacer::fill()),
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

        if stretch_clicked {
            cfg.ui_auto_stretch = !cfg.ui_auto_stretch;
        }
        if blur_clicked {
            cfg.ui_background_blur = !cfg.ui_background_blur;
        }
        cfg.font_scale = cfg.font_scale.clamp(0.5, 2.0);

        drop(guard);

        if want_back {
            Transition::Pop
        } else {
            Transition::None
        }
    }
}

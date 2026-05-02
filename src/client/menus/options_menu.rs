//! Options screen — direct mirror of `old/src/menus/options_menu.cpp`.
//!
//! Layout (top-down):
//!   * caption row
//!   * FOV slider | mouse-sensitivity slider
//!   * render-distance slider | "render options" sub-menu button
//!   * "language menu" sub-menu button | UI-scale slider
//!   * theme toggle | (filler)
//!   * flex spacer
//!   * back | save
//!
//! All slider value labels follow the C++ convention: the i18n key holds
//! the prefix (e.g. `"Field of view: "`) and the live value is appended at
//! the end. Sliders bind directly to the live `Config` so changes propagate
//! to the camera / world / surface every frame via `App::apply_config`.

use std::sync::{Arc, Mutex};

use egui::Context;

use super::screen::{Screen, Transition};
use super::{
    LanguageScreen, MENU_COL_SPACING, MENU_MAX_WIDTH, MENU_PADDING, MENU_ROW_HEIGHT,
    MENU_ROW_SPACING, RenderOptionsScreen, t,
};
use crate::config::Config;
use crate::globalization::I18n;
use crate::ui;
use crate::ui::widgets::{
    Aligned, Alignment, Button, CrossAxisSize, Flex, FlexItem, Label, MainAxisAlignment,
    MainAxisSize, Padding, Sizer, Slider, Spacer,
};

/// Top-level options screen.
///
/// `pending_ui_scale` mirrors `Config::ui_scale` and is what the slider
/// actually mutates. The change is committed back into `Config` only
/// when the user leaves this screen (via Back or Save) — otherwise the
/// whole UI would re-layout on every drag tick, which is jarring.
pub struct OptionsScreen {
    config: Arc<Mutex<Config>>,
    i18n: Arc<Mutex<I18n>>,
    pending_ui_scale: f32,
}

impl OptionsScreen {
    
    pub fn new(config: Arc<Mutex<Config>>, i18n: Arc<Mutex<I18n>>) -> Self {
        let pending_ui_scale = config.lock().map_or(1.0, |c| c.ui_scale);
        Self {
            config,
            i18n,
            pending_ui_scale,
        }
    }
}

impl Screen for OptionsScreen {
    fn title(&self) -> &'static str {
        "Options"
    }

    fn show(&mut self, ctx: &Context) -> Transition {
        let caption = t(&self.i18n, "NEWorld.options.caption");
        let fov_lbl = t(&self.i18n, "NEWorld.options.fov");
        let sens_lbl = t(&self.i18n, "NEWorld.options.sensitivity");
        let dist_lbl = t(&self.i18n, "NEWorld.options.distance");
        let render_lbl = t(&self.i18n, "NEWorld.options.rendermenu");
        let lang_lbl = t(&self.i18n, "NEWorld.options.languagemenu");
        let scale_lbl = t(&self.i18n, "NEWorld.options.ui_scale");
        let theme_lbl = t(&self.i18n, "NEWorld.options.theme");
        let theme_dark_lbl = t(&self.i18n, "NEWorld.options.theme_dark");
        let theme_light_lbl = t(&self.i18n, "NEWorld.options.theme_light");
        let back_lbl = t(&self.i18n, "NEWorld.options.back");
        let save_lbl = t(&self.i18n, "NEWorld.options.save");

        let mut want_save = false;
        let mut want_back = false;
        let mut want_render = false;
        let mut want_lang = false;
        let mut theme_clicked = false;
        // Sliders mutate cfg / pending state in place; the `_changed`
        // flags are unused — we don't currently need to react to slider
        // changes mid-frame.
        let mut fov_changed = false;
        let mut sens_changed = false;
        let mut dist_changed = false;
        let mut ui_scale_changed = false;

        let mut guard = self.config.lock().expect("config poisoned");
        // Reborrow to `&mut Config` so the multiple slider widgets below
        // can split-borrow disjoint fields. `MutexGuard`'s `DerefMut`
        // produces a fresh borrow per call which prevents simultaneous
        // field reborrows; reborrowing once via `&mut *guard` lets the
        // borrow checker see the fields as disjoint.
        let cfg: &mut Config = &mut guard;

        let theme_text = format!(
            "{theme_lbl}: {}",
            if cfg.dark_theme {
                &theme_dark_lbl
            } else {
                &theme_light_lbl
            }
        );

        let body = Flex::column(vec![
            // Caption.
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Aligned::center(Label::new(&caption)),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Row 1: FOV | mouse sensitivity
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(
                        1.0,
                        Slider::new(&fov_lbl, &mut cfg.fov_y_normal, 60.0..=120.0)
                            .changed(&mut fov_changed),
                    ),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(
                        1.0,
                        Slider::new(&sens_lbl, &mut cfg.mouse_speed, 0.01..=0.5)
                            .changed(&mut sens_changed),
                    ),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Row 2: render distance | render options sub-menu
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(
                        1.0,
                        Slider::new(&dist_lbl, &mut cfg.render_distance, 4..=48)
                            .changed(&mut dist_changed),
                    ),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(1.0, Button::new(&render_lbl).clicked(&mut want_render)),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Row 3: language sub-menu | font scale slider
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(
                        1.0,
                        Slider::new(&scale_lbl, &mut self.pending_ui_scale, 0.5..=2.0)
                            .changed(&mut ui_scale_changed),
                    ),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(1.0, Button::new(&lang_lbl).clicked(&mut want_lang)),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            )),
            FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
            // Row 4: theme toggle | (filler)
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(1.0, Button::new(&theme_text).clicked(&mut theme_clicked)),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(1.0, Spacer::fill()),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            )),
            // Flex spacer pushes the footer to the bottom.
            FlexItem::flex(1.0, Spacer::fill()),
            // Footer: Back | Save
            FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(1.0, Button::new(&back_lbl).clicked(&mut want_back)),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(1.0, Button::new(&save_lbl).clicked(&mut want_save)),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
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

        // Defensive clamps in case any TextEdit-backed widget is added
        // later that allows out-of-range typing.
        cfg.fov_y_normal = cfg.fov_y_normal.clamp(60.0, 120.0);
        cfg.mouse_speed = cfg.mouse_speed.clamp(0.01, 0.5);
        cfg.render_distance = cfg.render_distance.clamp(4, 48);
        // `pending_ui_scale` lives outside `cfg` until the user leaves
        // the screen; only clamp the local value here.
        self.pending_ui_scale = self.pending_ui_scale.clamp(0.5, 2.0);

        // The theme toggle takes effect immediately — `App::apply_config`
        // re-reads `cfg.dark_theme` every frame and pushes the matching
        // `Visuals` into the egui context.
        if theme_clicked {
            cfg.dark_theme = !cfg.dark_theme;
        }

        // Commit the pending UI scale to the live config when the user
        // leaves the screen (via Back or Save). Going into a sub-screen
        // (Push) does NOT commit — the slider position is preserved on
        // the OptionsScreen instance and the user can keep tweaking it
        // when they return.
        if want_back || want_save {
            cfg.ui_scale = self.pending_ui_scale;
        }

        // Snapshot the config we'll need for "Save"; release the lock before
        // we touch i18n / push transitions / write to disk.
        let snapshot_for_save = if want_save { Some(cfg.clone()) } else { None };
        drop(guard);

        if let Some(snap) = snapshot_for_save {
            let path = config_path();
            if let Err(err) = snap.save_to(&path) {
                tracing::warn!(error = %err, ?path, "options save failed");
            } else {
                tracing::info!(?path, "options saved");
            }
        }

        if want_back {
            Transition::Pop
        } else if want_render {
            Transition::Push(Box::new(RenderOptionsScreen::new(
                Arc::clone(&self.config),
                Arc::clone(&self.i18n),
            )))
        } else if want_lang {
            Transition::Push(Box::new(LanguageScreen::new(
                Arc::clone(&self.config),
                Arc::clone(&self.i18n),
            )))
        } else {
            Transition::None
        }
    }
}

/// Mirror `App::config_path` — the same dev-vs-deploy resolution. Re-derived
/// here because `App` doesn't expose its private path helper, and "Save
/// options" needs to write to the same location the boot reads from.
fn config_path() -> std::path::PathBuf {
    if let Some(dir) = option_env!("CARGO_MANIFEST_DIR") {
        return std::path::PathBuf::from(dir)
            .join("configs")
            .join("options.toml");
    }
    std::path::PathBuf::from(crate::config::DEFAULT_PATH)
}

//! Options screen — direct mirror of `old/src/menus/options_menu.cpp`.
//!
//! Layout (top-down):
//!   * caption row
//!   * FOV slider | mouse-sensitivity slider
//!   * render-distance slider | "render options" sub-menu button
//!   * "UI options" sub-menu button | "language menu" sub-menu button
//!   * flex spacer
//!   * back | save
//!
//! All slider value labels follow the C++ convention: the i18n key holds
//! the prefix (e.g. `"Field of view: "`) and the live value is appended at
//! the end. Sliders bind directly to the live `Config` so changes propagate
//! to the camera / world / surface every frame via `App::apply_config`.

use std::sync::{Arc, Mutex};

use egui::Context;

use super::{
    LanguageScreen, RenderOptionsScreen, UIOptionsScreen, caption_row, flex_spacer, footer_height,
    menu_panel, pair_row, t, MENU_ROW_SPACING,
};
use crate::config::Config;
use crate::globalization::I18n;
use crate::ui::screen::{Screen, Transition};

/// Top-level options screen.
pub struct OptionsScreen {
    config: Arc<Mutex<Config>>,
    i18n: Arc<Mutex<I18n>>,
}

impl OptionsScreen {
    #[must_use]
    pub fn new(config: Arc<Mutex<Config>>, i18n: Arc<Mutex<I18n>>) -> Self {
        Self { config, i18n }
    }
}

impl Screen for OptionsScreen {
    fn title(&self) -> &'static str {
        "Options"
    }

    fn ui(&mut self, ctx: &Context) -> Transition {
        let caption = t(&self.i18n, "NEWorld.options.caption");
        let fov_lbl = t(&self.i18n, "NEWorld.options.fov");
        let sens_lbl = t(&self.i18n, "NEWorld.options.sensitivity");
        let dist_lbl = t(&self.i18n, "NEWorld.options.distance");
        let render_lbl = t(&self.i18n, "NEWorld.options.rendermenu");
        let gui_lbl = t(&self.i18n, "NEWorld.options.guimenu");
        let lang_lbl = t(&self.i18n, "NEWorld.options.languagemenu");
        let back_lbl = t(&self.i18n, "NEWorld.options.back");
        let save_lbl = t(&self.i18n, "NEWorld.options.save");

        let mut transition = Transition::None;
        let mut want_save = false;
        let mut want_back = false;
        let mut want_render = false;
        let mut want_gui = false;
        let mut want_lang = false;

        let mut cfg = self.config.lock().expect("config poisoned");

        menu_panel(ctx, |ui| {
            caption_row(ui, &caption);
            ui.add_space(MENU_ROW_SPACING);

            // Row 1: FOV slider | mouse sensitivity slider
            pair_row(ui, |cols| {
                cols[0].label(format!("{fov_lbl}{:.0}", cfg.fov_y_normal));
                cols[0].add(egui::Slider::new(&mut cfg.fov_y_normal, 60.0..=120.0).show_value(false));
                cols[1].label(format!("{sens_lbl}{:.2}", cfg.mouse_speed));
                cols[1].add(egui::Slider::new(&mut cfg.mouse_speed, 0.01..=0.5).show_value(false));
            });
            ui.add_space(MENU_ROW_SPACING);

            // Row 2: render distance slider | render options sub-menu
            pair_row(ui, |cols| {
                cols[0].label(format!("{dist_lbl}{}", cfg.render_distance));
                cols[0].add(egui::Slider::new(&mut cfg.render_distance, 4..=48).show_value(false));
                if cols[1].button(&render_lbl).clicked() {
                    want_render = true;
                }
            });
            ui.add_space(MENU_ROW_SPACING);

            // Row 3: UI options | Language sub-menu
            pair_row(ui, |cols| {
                if cols[0].button(&gui_lbl).clicked() {
                    want_gui = true;
                }
                if cols[1].button(&lang_lbl).clicked() {
                    want_lang = true;
                }
            });
            ui.add_space(MENU_ROW_SPACING);

            // Footer is one pair_row (32 px). Reserve that height + a hair
            // for the row's own padding so it lands just above the bottom
            // panel edge.
            flex_spacer(ui, footer_height(1));

            // Footer: Back | Save
            pair_row(ui, |cols| {
                if cols[0].button(&back_lbl).clicked() {
                    want_back = true;
                }
                if cols[1].button(&save_lbl).clicked() {
                    want_save = true;
                }
            });

            // Defensive clamps in case any TextEdit-backed widget is added
            // later that allows out-of-range typing.
            cfg.fov_y_normal = cfg.fov_y_normal.clamp(60.0, 120.0);
            cfg.mouse_speed = cfg.mouse_speed.clamp(0.01, 0.5);
            cfg.render_distance = cfg.render_distance.clamp(4, 48);
        });

        // Snapshot the config we'll need for "Save"; release the lock before
        // we touch the i18n / push transitions / write to disk.
        let snapshot_for_save = if want_save { Some(cfg.clone()) } else { None };
        drop(cfg);

        if let Some(snap) = snapshot_for_save {
            // Save options to disk. Errors are logged but don't affect the UI;
            // the config edits remain in memory and will retry on next exit.
            let path = config_path();
            if let Err(err) = snap.save_to(&path) {
                tracing::warn!(error = %err, ?path, "options save failed");
            } else {
                tracing::info!(?path, "options saved");
            }
        }

        if want_back {
            transition = Transition::Pop;
        } else if want_render {
            transition = Transition::Push(Box::new(RenderOptionsScreen::new(
                Arc::clone(&self.config),
                Arc::clone(&self.i18n),
            )));
        } else if want_gui {
            transition = Transition::Push(Box::new(UIOptionsScreen::new(
                Arc::clone(&self.config),
                Arc::clone(&self.i18n),
            )));
        } else if want_lang {
            transition = Transition::Push(Box::new(LanguageScreen::new(
                Arc::clone(&self.config),
                Arc::clone(&self.i18n),
            )));
        }

        transition
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

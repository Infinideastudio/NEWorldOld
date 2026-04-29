//! Title screen — direct mirror of `old/src/menus/main_menu.cpp`.
//!
//! Layout (top-down): a 128-px-tall banner, optional "Back to game" button
//! (only when a game is loaded), "Start game", a paired Options / Exit row,
//! and a bottom-left help label that lives as its own corner overlay.
//!
//! Built against [`crate::ui::widgets`]: column distribution + `main_align =
//! Center` vertically centres the whole block; the help label sits in a
//! separate `egui::Area` so it can anchor to the corner of the window
//! without competing with the menu's own constraint flow.

use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use egui::{Align2, Color32, Context, RichText, TextureId, vec2};

use super::action::WorldActionQueue;
use super::screen::{Screen, Transition};
use super::{
    MENU_COL_SPACING, MENU_MAX_WIDTH, MENU_PADDING, MENU_ROW_HEIGHT, MENU_ROW_SPACING,
    OptionsScreen, WorldSelectScreen, t,
};
use crate::config::Config;
use crate::globalization::I18n;
use crate::ui;
use crate::ui::widgets::{
    Aligned, Alignment, BoxFit, Button, CrossAxisSize, Flex, FlexItem, Image, MainAxisAlignment,
    MainAxisSize, Padding, Sizer, Spacer,
};

/// Title-screen banner height in logical pixels — matches the C++
/// `Sizer({.max_height = 256})` row that hosts the C++ `TitleTexture`
/// `ImageBox` (see `old/src/menus/main_menu.cpp:18`).
const BANNER_HEIGHT: f32 = 256.0;

/// The main title screen. Always sits at the bottom of the screen stack
/// when no world is loaded; pushed back on top from the in-game pause menu
/// after `WorldAction::LeaveToTitle` has torn the world down.
pub struct TitleScreen {
    /// Shared with the App + every other screen — `Options` mutates it
    /// directly.
    config: Arc<Mutex<Config>>,
    /// Active language table, threaded into every menu so labels stay in
    /// sync after the language picker reloads.
    i18n: Arc<Mutex<I18n>>,
    /// Where the world list and "create world" screen anchor their disk I/O.
    /// Threaded through so the screens don't have to duplicate the dev-vs-
    /// release path resolution from [`crate::menus::action::default_worlds_root`].
    worlds_root: PathBuf,
    /// Mailbox for cross-screen world-lifecycle requests (open / leave /
    /// delete). The world-select screen sends `Enter`; the app drains.
    actions: Arc<WorldActionQueue>,
    /// `egui::TextureId` for the rendered `assets/textures/ui/title.png`.
    /// Painted by an [`Image`] widget on the banner row.
    title_icon: TextureId,
    /// Source PNG dimensions of the title texture, used by the [`Image`]
    /// widget for aspect-ratio fit math.
    title_icon_size: (u32, u32),
}

impl TitleScreen {
    #[must_use]
    pub fn new(
        config: Arc<Mutex<Config>>,
        i18n: Arc<Mutex<I18n>>,
        worlds_root: PathBuf,
        actions: Arc<WorldActionQueue>,
        title_icon: TextureId,
        title_icon_size: (u32, u32),
    ) -> Self {
        Self {
            config,
            i18n,
            worlds_root,
            actions,
            title_icon,
            title_icon_size,
        }
    }
}

impl Screen for TitleScreen {
    fn title(&self) -> &'static str {
        "Title"
    }

    fn show(&mut self, ctx: &Context) -> Transition {
        let start_label = t(&self.i18n, "NEWorld.main.start");
        let options_label = t(&self.i18n, "NEWorld.main.options");
        let exit_label = t(&self.i18n, "NEWorld.main.exit");
        let help_label = t(&self.i18n, "NEWorld.main.help");

        let mut start_clicked = false;
        let mut options_clicked = false;
        let mut exit_clicked = false;

        let body = Flex::column(vec![
            (FlexItem::new(Sizer::height(
                BANNER_HEIGHT,
                Image::new(
                    self.title_icon,
                    self.title_icon_size.0 as f32,
                    self.title_icon_size.1 as f32,
                )
                .fit(BoxFit::Contain)
                .alignment(Alignment::Center),
            ))),
            (FlexItem::new(Spacer::height(MENU_ROW_SPACING))),
            (FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Button::new(start_label, &mut start_clicked),
            ))),
            (FlexItem::new(Spacer::height(MENU_ROW_SPACING))),
            (FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Flex::row(vec![
                    FlexItem::flex(1.0, Button::new(options_label, &mut options_clicked)),
                    FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                    FlexItem::flex(1.0, Button::new(exit_label, &mut exit_clicked)),
                ])
                .main_size(MainAxisSize::Max)
                .cross_size(CrossAxisSize::Max),
            ))),
        ])
        .main_size(MainAxisSize::Max)
        .main_align(MainAxisAlignment::Center)
        .cross_size(CrossAxisSize::Max);

        let root = Aligned::new(
            Alignment::TopCenter,
            Padding::all(MENU_PADDING, Sizer::width(MENU_MAX_WIDTH, body)),
        );

        ui::show(ctx, root);

        // Help line anchored to the bottom-left of the whole window —
        // mirrors the C++ `StackItem({.alignment = BOTTOM_LEFT}, …)`. Lives
        // outside the layout root because it's positioned relative to the
        // viewport, not the centred menu column.
        if !help_label.is_empty() {
            egui::Area::new("title.help".into())
                .anchor(Align2::LEFT_BOTTOM, vec2(8.0, -8.0))
                .interactable(false)
                .show(ctx, |ui| {
                    ui.label(
                        RichText::new(help_label)
                            .color(Color32::from_gray(200))
                            .size(13.0),
                    );
                });
        }

        if start_clicked {
            Transition::Push(Box::new(WorldSelectScreen::new(
                self.worlds_root.clone(),
                Arc::clone(&self.i18n),
                Arc::clone(&self.actions),
            )))
        } else if options_clicked {
            Transition::Push(Box::new(OptionsScreen::new(
                Arc::clone(&self.config),
                Arc::clone(&self.i18n),
            )))
        } else if exit_clicked {
            Transition::Exit
        } else {
            Transition::None
        }
    }
}

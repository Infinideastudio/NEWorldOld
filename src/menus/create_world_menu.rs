//! Create world screen — direct mirror of `old/src/menus/create_world_menu.cpp`.
//!
//! Layout: caption row, optional error label (red), single-line text-edit
//! row for the world name, then a Back / OK pair row. The C++ build
//! creates the world directory inline on "OK" and pops back; we do the
//! same. Validation errors (empty name, illegal characters, name
//! collision) appear above the form in red.

use std::path::PathBuf;
use std::sync::{Arc, Mutex};

use egui::{Color32, Context};

use super::action::WorldActionQueue;
use super::screen::{Screen, Transition};
use super::{MENU_COL_SPACING, MENU_MAX_WIDTH, MENU_PADDING, MENU_ROW_HEIGHT, MENU_ROW_SPACING, t};
use crate::globalization::I18n;
use crate::ui;
use crate::ui::widgets::{
    Aligned, Alignment, Button, CrossAxisSize, Flex, FlexItem, Label, MainAxisSize, Padding, Sizer,
    Spacer, TextEdit,
};

/// Create world form state.
pub struct CreateWorldScreen {
    worlds_root: PathBuf,
    i18n: Arc<Mutex<I18n>>,
    /// Held in case a future iteration wants to immediately submit
    /// `WorldAction::Enter` after creating, instead of bouncing back through
    /// the world list. Today we just pop, so the field is unused at runtime.
    #[allow(dead_code)]
    actions: Arc<WorldActionQueue>,
    world_name: String,
    /// Set to a human-readable error message if the most recent Create
    /// attempt failed. Shown inline above the form.
    error: Option<String>,
}

impl CreateWorldScreen {
    #[must_use]
    pub fn new(
        worlds_root: PathBuf,
        i18n: Arc<Mutex<I18n>>,
        actions: Arc<WorldActionQueue>,
    ) -> Self {
        Self {
            worlds_root,
            i18n,
            actions,
            world_name: String::new(),
            error: None,
        }
    }

    /// Validate `name` and create the world directory if it doesn't already
    /// exist. Returns `true` on success.
    fn try_create(&mut self) -> bool {
        let name = self.world_name.trim();
        if name.is_empty() {
            self.error = Some("World name can't be empty.".to_owned());
            return false;
        }
        if !is_safe_world_name(name) {
            self.error = Some(
                "Use only letters, numbers, dashes and underscores in the world name.".to_owned(),
            );
            return false;
        }
        // Path convention: worlds live at `<worlds_root>/worlds/<name>/`,
        // matching `World::new_at`'s internal layout. `worlds_root` is the
        // parent dir (crate dir in dev), not the worlds dir itself.
        let dir = self.worlds_root.join("worlds").join(name);
        if dir.exists() {
            self.error = Some("A world with that name already exists.".to_owned());
            return false;
        }
        if let Err(err) = std::fs::create_dir_all(&dir) {
            self.error = Some(format!("Failed to create world directory: {err}"));
            return false;
        }
        self.error = None;
        true
    }
}

impl Screen for CreateWorldScreen {
    fn title(&self) -> &'static str {
        "Create World"
    }

    fn show(&mut self, ctx: &Context) -> Transition {
        let caption = t(&self.i18n, "NEWorld.create.caption");
        let placeholder = t(&self.i18n, "NEWorld.create.inputname");
        let ok_label = t(&self.i18n, "NEWorld.create.ok");
        let back_label = t(&self.i18n, "NEWorld.create.back");

        let mut create_clicked = false;
        let mut cancel_clicked = false;
        let mut text_submitted = false;

        // Build the body. The error row is only present when an error is
        // set; absent rows just don't appear in the column.
        let mut body_items: Vec<FlexItem> = Vec::new();
        body_items.push(FlexItem::new(Sizer::height(
            MENU_ROW_HEIGHT,
            Aligned::center(Label::new(&caption)),
        )));
        body_items.push(FlexItem::new(Spacer::height(MENU_ROW_SPACING)));
        if let Some(err) = &self.error {
            body_items.push(FlexItem::new(Sizer::height(
                MENU_ROW_HEIGHT,
                Aligned::center(Label::new(err).color(Color32::LIGHT_RED)),
            )));
            body_items.push(FlexItem::new(Spacer::height(MENU_ROW_SPACING)));
        }
        body_items.push(FlexItem::new(Sizer::height(
            MENU_ROW_HEIGHT,
            TextEdit::new(&mut self.world_name)
                .hint(placeholder)
                .submitted(&mut text_submitted),
        )));
        body_items.push(FlexItem::new(Spacer::height(MENU_ROW_SPACING)));
        body_items.push(FlexItem::new(Sizer::height(
            MENU_ROW_HEIGHT,
            Flex::row(vec![
                FlexItem::flex(1.0, Button::new(&back_label).clicked(&mut cancel_clicked)),
                FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                FlexItem::flex(1.0, Button::new(&ok_label).clicked(&mut create_clicked)),
            ])
            .main_size(MainAxisSize::Max)
            .cross_size(CrossAxisSize::Max),
        )));

        let body = Flex::column(body_items)
            .main_size(MainAxisSize::Min)
            .cross_size(CrossAxisSize::Max);

        let root = Aligned::new(
            Alignment::Center,
            Padding::all(MENU_PADDING, Sizer::width(MENU_MAX_WIDTH, body)),
        );

        ui::show(ctx, root);

        // Pressing Enter inside the text edit submits, same as clicking OK.
        let submit = create_clicked || text_submitted;

        if cancel_clicked || (submit && self.try_create()) {
            Transition::Pop
        } else {
            Transition::None
        }
    }
}

/// Restrict world names to a safe alphabet so we never have to worry about
/// path-traversal sequences (`..`), reserved Windows names, or unsafe
/// punctuation. Empty input is rejected separately.
fn is_safe_world_name(name: &str) -> bool {
    name.chars()
        .all(|c| c.is_ascii_alphanumeric() || c == '-' || c == '_')
}

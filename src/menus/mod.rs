//! Menus — direct mirror of `src/menus/` in the C++ tree.
//!
//! Each file maps one-to-one with the matching C++ source so a feature can
//! be diffed across builds:
//!
//! | C++ file                       | Rust file                 |
//! |--------------------------------|---------------------------|
//! | `main_menu.cpp`                | [`main_menu`]             |
//! | `world_menu.cpp`               | [`world_menu`]            |
//! | `create_world_menu.cpp`        | [`create_world_menu`]     |
//! | `options_menu.cpp`             | [`options_menu`]          |
//! | `render_options_menu.cpp`      | [`render_options_menu`]   |
//! | `shader_options_menu.cpp`      | [`shader_options_menu`]   |
//! | `ui_options_menu.cpp`          | [`ui_options_menu`]       |
//! | `language_menu.cpp`            | [`language_menu`]         |
//! | `game_menu.cpp` (pause)        | [`game_menu`]             |
//!
//! Note: in C++ the in-game HUD and inventory rendering live inside
//! `neworld.ixx`'s `draw_hud` / `draw_inventory` functions, so the Rust
//! `game_menu.rs` covers a slightly larger surface than its C++ counterpart
//! (it composes the HUD + inventory + pause menu into one in-game screen).
//! The pause-menu UI alone matches the C++ `menus/game_menu.cpp` shape.
//!
//! ## Layout chrome
//!
//! Every C++ menu sits inside an identical outer chrome — a centered column
//! padded by 32 px with a maximum width of 512 px, a 32-px-tall caption row
//! at the top, and a back / save row pinned at the bottom by a flex-grow
//! spacer. The Rust menus reproduce this with [`MenuChrome`] / the
//! [`menu_panel`] helper so each screen reads as a sequence of body rows
//! plus a footer.

pub mod create_world_menu;
pub mod game_menu;
pub mod language_menu;
pub mod main_menu;
pub mod options_menu;
pub mod render_options_menu;
pub mod shader_options_menu;
pub mod ui_options_menu;
pub mod world_menu;

pub use create_world_menu::CreateWorldScreen;
pub use game_menu::GameScreen;
pub use language_menu::LanguageScreen;
pub use main_menu::TitleScreen;
pub use options_menu::OptionsScreen;
pub use render_options_menu::RenderOptionsScreen;
pub use shader_options_menu::ShaderOptionsScreen;
pub use ui_options_menu::UIOptionsScreen;
pub use world_menu::WorldSelectScreen;

use std::sync::{Arc, Mutex};

use egui::{Color32, Context, FontId, Layout, RichText, Ui, vec2};

use crate::globalization::I18n;

/// Match the C++ menu chrome from `old/src/menus/*.cpp`:
/// `Padding { 32, 32, 32, 32 }` around a `Sizer { max_width = 512 }` column.
pub const MENU_MAX_WIDTH: f32 = 512.0;
/// Opaque background fill for every menu panel. Dark so the menus read
/// clearly when no world is loaded behind them and so the in-game pause
/// path doesn't bleed the live render through.
pub const MENU_BG: Color32 = Color32::from_rgb(20, 22, 28);
/// Outer padding — matches the C++ `Padding({.left=32,.top=32,.right=32,.bottom=32})`.
pub const MENU_PADDING: f32 = 32.0;
/// Standard row height — matches the C++ `Sizer({.max_height = 32})`.
pub const MENU_ROW_HEIGHT: f32 = 32.0;
/// Inter-row vertical spacing — matches the C++ `Spacer({.height = 8})`.
pub const MENU_ROW_SPACING: f32 = 8.0;
/// Inter-column spacing inside a paired-button row — matches `Spacer({.width = 8})`.
pub const MENU_COL_SPACING: f32 = 8.0;

/// Build the standard centred menu container and call `body` to fill in the
/// rows. Mirrors the C++ outer wrapper:
///
/// ```cpp
/// Row({.main_axis_alignment = CENTER},
///   FlexItem({.flex_grow = 1},
///     Padding({.left=32,.top=32,.right=32,.bottom=32},
///       Sizer({.max_width = 512}, std::move(column)))))
/// ```
#[allow(deprecated)] // CentralPanel::show
pub fn menu_panel(ctx: &Context, body: impl FnOnce(&mut Ui)) {
    egui::CentralPanel::default()
        .frame(
            egui::Frame::default()
                .inner_margin(MENU_PADDING)
                .fill(MENU_BG),
        )
        .show(ctx, |ui| {
            // Snapshot the full inner area *before* allocating any sub-Uis.
            // We need both the width (for centring) and the height (so
            // `flex_spacer` can compute "remaining space" correctly inside
            // `body`). `ui.horizontal(...)` would leave `available_height()`
            // unreliable inside the inner column, so we allocate the whole
            // inner area as a single horizontal Ui ourselves.
            let avail = ui.available_size();
            let column_w = avail.x.min(MENU_MAX_WIDTH);
            let side_pad = ((avail.x - column_w) * 0.5).max(0.0);
            ui.allocate_ui_with_layout(
                avail,
                Layout::left_to_right(egui::Align::Min),
                |ui| {
                    ui.add_space(side_pad);
                    // Inner top-down column. The vec2 sizes the child's
                    // `max_rect`, so `available_height()` inside `body`
                    // starts at `avail.y` and shrinks as rows are added —
                    // exactly what `flex_spacer` needs to push the footer
                    // to the bottom.
                    ui.allocate_ui_with_layout(
                        vec2(column_w, avail.y),
                        Layout::top_down(egui::Align::Min),
                        |ui| {
                            ui.set_min_size(vec2(column_w, avail.y));
                            // Same per-column style overrides as `pair_row`
                            // so full-width buttons and sliders are sized
                            // consistently across nested cells.
                            let s = ui.spacing_mut();
                            s.interact_size.y = MENU_ROW_HEIGHT;
                            s.slider_width = column_w;
                            body(ui);
                        },
                    );
                },
            );
        });
}

/// Push the rest of the column to the bottom, leaving `reserved_below`
/// pixels of vertical space for the trailing footer rows the caller is
/// about to add. Mirrors the C++ `FlexItem({.flex_grow = 1}, Spacer({.height
/// = inf}))` idiom; the C++ build sizes the footer with a `Sizer` and
/// `Spacer` directly underneath, so the "leftover space" calculation is
/// done by the layout engine for free — egui doesn't have a flex-grow
/// equivalent, so we compute it here.
///
/// Pass the total *inner* height of the footer (rows × `MENU_ROW_HEIGHT`
/// plus inter-row `MENU_ROW_SPACING` per gap, no trailing spacing).
pub fn flex_spacer(ui: &mut Ui, reserved_below: f32) {
    let h = (ui.available_height() - reserved_below).max(0.0);
    ui.add_space(h);
}

/// Convenience: total height of a footer made of `rows` standard
/// row-height rows separated by `MENU_ROW_SPACING`. Use as the
/// `reserved_below` argument to [`flex_spacer`].
#[must_use]
pub fn footer_height(rows: usize) -> f32 {
    if rows == 0 {
        return 0.0;
    }
    let n = rows as f32;
    n * MENU_ROW_HEIGHT + (n - 1.0) * MENU_ROW_SPACING
}

/// 32-px-tall row containing a single centred caption label. Mirrors:
///
/// ```cpp
/// Sizer({.max_height = 32}, Center({}, Label(text)))
/// ```
pub fn caption_row(ui: &mut Ui, text: &str) {
    ui.allocate_ui_with_layout(
        vec2(ui.available_width(), MENU_ROW_HEIGHT),
        Layout::centered_and_justified(egui::Direction::TopDown),
        |ui| {
            ui.label(
                RichText::new(text)
                    .font(FontId::proportional(18.0))
                    .strong(),
            );
        },
    );
}

/// Render a single full-width button at row height. Returns whether it was
/// clicked. Mirrors `Sizer({.max_height = 32}, Button({.label = …}))`.
pub fn full_row_button(ui: &mut Ui, text: &str) -> bool {
    let mut clicked = false;
    ui.allocate_ui_with_layout(
        vec2(ui.available_width(), MENU_ROW_HEIGHT),
        Layout::centered_and_justified(egui::Direction::LeftToRight),
        |ui| {
            if ui.button(text).clicked() {
                clicked = true;
            }
        },
    );
    clicked
}

/// Render two equal-width columns side by side. Mirrors:
///
/// ```cpp
/// Sizer({.max_height = 32},
///   Row({.main_axis_size = MAX},
///     FlexItem({.flex_grow = 1}, left),
///     Spacer({.width = 8}),
///     FlexItem({.flex_grow = 1}, right)))
/// ```
///
/// Body receives `cols: &mut [Ui; 2]`; index `cols[0]` for the left column
/// and `cols[1]` for the right. The body closure has a single borrow of
/// any shared state (e.g. the locked `Config`), avoiding the
/// double-`&mut` capture conflict you'd get with two separate `FnOnce`s.
///
/// Each column uses `Layout::top_down_justified(Align::Center)`:
/// * `top_down`            — children stack vertically (label-then-slider
///   patterns work).
/// * `justified`           — horizontal stretching, so a `ui.button(text)`
///   fills the column width.
/// * `Align::Center`       — cross-axis (horizontal) alignment is centre,
///   which is what `Button::ui` reads via
///   `ui.layout().align_size_within_rect(galley, rect)` to position its
///   label. Result: button text is centred horizontally without making
///   the button take the entire remaining vertical space (which
///   `centered_and_justified` would do).
pub fn pair_row(ui: &mut Ui, body: impl FnOnce(&mut [Ui; 2])) {
    let spacing = MENU_COL_SPACING;
    let total = ui.available_width();
    let column_width = ((total - spacing) * 0.5).max(0.0);
    let top_left = ui.cursor().min;
    let bottom = ui.max_rect().bottom();

    let column_layout = Layout::top_down_justified(egui::Align::Center);
    let mk = |ui: &mut Ui, col_idx: usize| {
        let x = top_left.x + (col_idx as f32) * (column_width + spacing);
        let rect = egui::Rect::from_min_max(
            egui::Pos2::new(x, top_left.y),
            egui::Pos2::new(x + column_width, bottom),
        );
        let mut col = ui.new_child(
            egui::UiBuilder::new()
                .max_rect(rect)
                .layout(column_layout),
        );
        col.set_width(column_width);
        // Per-column style overrides:
        //   * `interact_size.y = MENU_ROW_HEIGHT` makes every interactive
        //     widget (button, checkbox, …) at least 32 px tall. With the
        //     column's `[Center, Center]` alignment, the button rect is
        //     taller than the text galley, so the text actually appears
        //     vertically centred — without this, the natural button height
        //     collapses to font-height + small padding and Unifont's high
        //     baseline pulls the visible text up.
        //   * `slider_width = column_width` makes a `Slider` widget's track
        //     stretch to fill the column instead of using egui's default
        //     ~100 px width.
        let s = col.spacing_mut();
        s.interact_size.y = MENU_ROW_HEIGHT;
        s.slider_width = column_width;
        col
    };

    let mut cols = [mk(ui, 0), mk(ui, 1)];
    body(&mut cols);

    // Advance the parent cursor by the taller of the two columns so the
    // next row starts below the pair (mirrors `ui.columns`'s behaviour).
    let max_h = cols[0].min_rect().height().max(cols[1].min_rect().height());
    ui.advance_cursor_after_rect(egui::Rect::from_min_size(
        top_left,
        vec2(total, max_h),
    ));
}

/// Snapshot helper: lock `i18n`, copy out a `String` for `key`. Used by the
/// menus so they don't have to hold the mutex for the duration of an egui
/// closure (egui callbacks don't tolerate lock guards alive across `&mut Ui`
/// reborrows).
pub fn t(i18n: &Arc<Mutex<I18n>>, key: &str) -> String {
    i18n.lock()
        .map(|i| i.get(key).to_owned())
        .unwrap_or_default()
}

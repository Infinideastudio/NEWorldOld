//! UI infrastructure — Rust-specific layout engine and widget set.
//!
//! Two halves:
//!
//! * [`layout`] — the core abstractions: geometry types ([`Point`],
//!   [`Size`], [`Constraint`], [`Alignment`]) and the [`Element`] trait
//!   that every widget implements. No widget-specific code lives here.
//! * [`widgets`] — the concrete elements: containers (Padding, Sizer,
//!   Spacer, Aligned, Flex, ScrollView) and atomic widget leaves (Label,
//!   Button, SelectButton, Slider, TextEdit), plus the top-level
//!   `run` / `run_overlay` entry points. Re-exports the core types from
//!   [`layout`] so menu authors can pull everything from one namespace.
//!
//! [`Point`]: layout::Point
//! [`Size`]: layout::Size
//! [`Constraint`]: layout::Constraint
//! [`Alignment`]: layout::Alignment
//! [`Element`]: layout::Element

use egui::{Align2, Color32, Context};

use layout::{Constraint, Element, Point};

pub mod layout;
pub mod widgets;

/// Build, layout, and show a single-frame view inside an opaque
/// `CentralPanel`. The root is taken by value (any concrete `E: Element`)
/// so no boxing happens at the top of the tree — it's monomorphised per
/// call site. Theming follows `ctx.global_style().visuals` (dark by
/// default in this app).
pub fn show<E: Element>(ctx: &Context, mut root: E) {
    // Layout the root against the whole screen - same as the content rect
    // of the CentralPanel we'll show it in.
    let rect = ctx.content_rect();
    let _size = root.layout(ctx, Constraint::new(rect.width(), rect.height()));

    // CentralPanel::show is the standard top-level entry; show_inside requires an existing Ui we don't have.
    #[allow(deprecated)]
    egui::CentralPanel::default().show(ctx, |ui| {
        let rect = ui.content_rect();
        root.show(ui, Point::new(rect.min.x, rect.min.y));
    });
}

/// Like [`show`] but puts the element inside a modal dialog — used for
/// in-game overlays (e.g. the pause menu) where the live world should
/// remain visible behind the UI.
///
/// The Window auto-sizes to fit the root: we lay the root out against
/// the screen-sized constraint, allocate that exact size in the Window's
/// Ui, then render the root at the allocated rect's origin. This keeps
/// the title bar from overlapping content (which it did when we ran the
/// layout pass before the Window opened — the Ui had no allocations and
/// the Window collapsed to title-bar height while our absolute-positioned
/// widgets drew elsewhere).
///
/// The supplied `root` is expected to have a *natural* size — i.e. don't
/// wrap it in `Aligned::Center` or anything else that fills the
/// constraint, since the Window already centres itself on screen.
pub fn show_modal<E: Element>(ctx: &Context, id: &'static str, mut root: E) {
    // Layout the root against the whole screen - the largest possible size
    // it could be.
    let rect = ctx.content_rect();
    let size = root.layout(ctx, Constraint::new(rect.width(), rect.height()));

    egui::Area::new(egui::Id::new(("neworld.modal.scrim", id)))
        .anchor(Align2::LEFT_TOP, egui::vec2(0.0, 0.0))
        .interactable(false)
        .order(egui::Order::Background)
        .show(ctx, |ui| {
            ui.painter()
                .rect_filled(rect, 0.0, Color32::from_black_alpha(120));
        });

    egui::Window::new(id)
        .title_bar(false)
        .collapsible(false)
        .resizable(false)
        .anchor(Align2::CENTER_CENTER, egui::vec2(0.0, 0.0))
        .show(ctx, |ui| {
            let (_, rect) = ui.allocate_space(egui::vec2(size.width, size.height));
            root.show(ui, Point::new(rect.min.x, rect.min.y));
        });
}

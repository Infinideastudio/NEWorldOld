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

use egui::{Color32, Context};

use layout::{Constraint, Element, Point};

pub mod layout;
pub mod widgets;

/// Build, layout, and show a single-frame view inside a transparent
/// `CentralPanel`. The root is taken by value (any concrete `E: Element`)
/// so no boxing happens at the top of the tree — it's monomorphised per
/// call site. Theming follows `ctx.global_style().visuals`.
///
/// The panel frame is forced to a transparent fill so whatever was drawn
/// to the surface before the egui pass — the rotating menu-background
/// (out-of-game) or the live world (paused in-game) — shows through. A
/// full-screen dimmer scrim is painted *under* the widget content (via
/// the same painter, before the layout root paints) so the backdrop
/// doesn't compete with text legibility. Scrim colour follows the
/// active theme; widgets sit on top because they're submitted to the
/// painter after the `rect_filled` call. Used by every menu screen
/// including the in-game pause menu.
pub fn show<E: Element>(ctx: &Context, mut root: E) {
    // Layout the root against the whole screen - same as the content rect
    // of the CentralPanel we'll show it in.
    let rect = ctx.content_rect();
    let _size = root.layout(ctx, Constraint::new(rect.width(), rect.height()));

    let scrim = scrim_color(ctx);

    // CentralPanel::show is the standard top-level entry; show_inside requires an existing Ui we don't have.
    #[allow(deprecated)]
    egui::CentralPanel::default()
        .frame(egui::Frame::default().fill(Color32::TRANSPARENT))
        .show(ctx, |ui| {
            let panel_rect = ui.content_rect();
            // Scrim first — paint into the CentralPanel's own layer so
            // subsequent widget draws in this same closure land on top
            // automatically.
            ui.painter().rect_filled(panel_rect, 0.0, scrim);
            root.show(ui, Point::new(panel_rect.min.x, panel_rect.min.y));
        });
}

/// Theme-dependent scrim colour. Dark theme → black-alpha (dim toward
/// black so dark widget chrome reads against it); light theme →
/// white-alpha (lift toward white so light widget chrome reads against
/// it).
fn scrim_color(ctx: &Context) -> Color32 {
    if ctx.global_style().visuals.dark_mode {
        Color32::from_black_alpha(128)
    } else {
        Color32::from_white_alpha(64)
    }
}

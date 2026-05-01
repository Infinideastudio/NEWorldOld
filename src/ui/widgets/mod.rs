//! Concrete elements built on top of [`crate::ui::layout`].
//!
//! Two kinds of element:
//! * **Layout-aware containers** ([`Spacer`], [`Sizer`], [`Padding`],
//!   [`Aligned`], [`Flex`], [`ScrollView`]): pure constraint algebra,
//!   painted via `egui::Painter`. Single-child containers are generic
//!   over their child and store it inline; `FlexItem` is the only boxing
//!   boundary.
//! * **Atomic-widget leaves** ([`Label`], [`Button`], [`SelectButton`],
//!   [`Slider`], [`TextEdit`]): wrap an egui widget and host it at the
//!   absolute rect we computed. egui keeps doing input/IME/focus and
//!   supplies all theming via its [`Style`](egui::Style) — we don't
//!   carry a separate theme.
//!
//! Interactive leaves don't take callbacks; they expose each event flag
//! — clicked, double-clicked, changed, submitted — as a builder method
//! that takes a `&mut bool`. Callers attach a slot only for whichever
//! events they care about; unset events are dropped silently. We
//! deliberately avoid storing `egui::Response` because Response holds a
//! `Context` clone whose interaction methods lock the context; calling
//! them later inside another context-locking closure (e.g. `ctx.input`)
//! can deadlock. Extracting bools at the call site eliminates that
//! surface entirely.
//!
//! The core types ([`Element`], [`Constraint`], [`Point`], [`Size`],
//! [`Alignment`]) are re-exported from [`crate::ui::layout`] so callers
//! can pull everything they need from this single namespace.

mod button;
mod containers;
mod flex;
mod image;
mod label;
mod scroll;
mod select_button;
mod slider;
mod text_edit;

pub use button::Button;
pub use containers::{Aligned, Padding, Sizer, Spacer};
pub use flex::{
    CrossAxisAlignment, CrossAxisSize, Flex, FlexDirection, FlexItem, MainAxisAlignment,
    MainAxisSize,
};
pub use image::{BoxFit, Image, apply_box_fit};
pub use label::Label;
pub use scroll::{ScrollDirection, ScrollView};
pub use select_button::SelectButton;
pub use slider::Slider;
pub use text_edit::TextEdit;

// Re-export the layout core so menu authors only have to import from
// `crate::ui::widgets::*` to get everything.
pub use crate::ui::layout::{Alignment, Constraint, Element, Point, Size};

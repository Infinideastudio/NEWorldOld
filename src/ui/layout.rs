//! Layout core — geometry types, the [`Element`] trait, and the small
//! set of helpers every concrete widget uses.
//!
//! The widget set itself lives next door in [`crate::ui::widgets`].
//! Anything that doesn't depend on a specific widget — constraints/sizes
//! algebra, alignment math, default-font resolution — sits here.

use egui::{Context, Pos2, Rect, Ui, Vec2};

/// Two-dimensional point.
#[derive(Debug, Clone, Copy, Default)]
pub struct Point {
    pub x: f32,
    pub y: f32,
}

impl Point {
    pub const fn new(x: f32, y: f32) -> Self {
        Self { x, y }
    }
}

/// Width and height of an element.
#[derive(Debug, Clone, Copy, Default)]
pub struct Size {
    pub width: f32,
    pub height: f32,
}

impl Size {
    pub const fn new(width: f32, height: f32) -> Self {
        Self { width, height }
    }
}

/// Maximum width and height a parent element allows.
#[derive(Debug, Clone, Copy, Default)]
pub struct Constraint {
    pub max_width: f32,
    pub max_height: f32,
}

impl Constraint {
    pub const fn new(max_width: f32, max_height: f32) -> Self {
        Self {
            max_width,
            max_height,
        }
    }

    /// The largest finite size satisfying the constraint.
    /// Infinite axes collapse to zero.
    pub fn max_size(self) -> Size {
        Size::new(
            if self.max_width.is_finite() {
                self.max_width
            } else {
                0.0
            },
            if self.max_height.is_finite() {
                self.max_height
            } else {
                0.0
            },
        )
    }
}

/// Layout/show contract.
///
/// `layout` runs first and returns the natural size given the constraint.
/// `show` runs after and paints + handles interaction inside the rect the
/// parent decides on (origin + the size returned from `layout`).
pub trait Element {
    fn layout(&mut self, ctx: &Context, c: Constraint) -> Size;
    fn show(&mut self, ui: &mut Ui, origin: Point);
}

/// Named alignment points for use by some elements.
#[derive(Copy, Clone, Debug)]
pub enum Alignment {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
}

impl Alignment {
    pub fn fractions(self) -> (f32, f32) {
        match self {
            Self::TopLeft => (0.0, 0.0),
            Self::TopCenter => (0.5, 0.0),
            Self::TopRight => (1.0, 0.0),
            Self::CenterLeft => (0.0, 0.5),
            Self::Center => (0.5, 0.5),
            Self::CenterRight => (1.0, 0.5),
            Self::BottomLeft => (0.0, 1.0),
            Self::BottomCenter => (0.5, 1.0),
            Self::BottomRight => (1.0, 1.0),
        }
    }
}

/// Convert from our geometry types to egui's for painting and hit-testing.
pub fn rect_at(origin: Point, size: Size) -> Rect {
    Rect::from_min_size(
        Pos2::new(origin.x, origin.y),
        Vec2::new(size.width, size.height),
    )
}

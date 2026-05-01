//! Single-child layout containers — pure constraint algebra, no input.
//!
//! All four are generic over their child and store it inline as a struct
//! field, so subtrees built out of these compose into one stack value
//! with zero allocations.

use egui::{Context, Ui};

use crate::ui::layout::{Alignment, Constraint, Element, Point, Size};

/// Whitespace element — fixed width and height, capped at the constraint.
pub struct Spacer {
    pub width: f32,
    pub height: f32,
}

impl Spacer {
    pub fn new(width: f32, height: f32) -> Self {
        Self { width, height }
    }

    pub fn height(height: f32) -> Self {
        Self { width: 0.0, height }
    }

    pub fn width(width: f32) -> Self {
        Self { width, height: 0.0 }
    }

    pub fn empty() -> Self {
        Self {
            width: 0.0,
            height: 0.0,
        }
    }

    pub fn fill() -> Self {
        Self {
            width: f32::INFINITY,
            height: f32::INFINITY,
        }
    }
}

impl Element for Spacer {
    fn layout(&mut self, _ctx: &Context, c: Constraint) -> Size {
        Size::new(
            self.width.min(c.max_width).max(0.0),
            self.height.min(c.max_height).max(0.0),
        )
    }

    fn show(&mut self, _ui: &mut Ui, _origin: Point) {}
}

/// Caps the constraint passed to a single inline child along one or both
/// axes. The child is free to use less than the cap; the cap only forbids
/// using more.
pub struct Sizer<E: Element> {
    pub max_width: f32,
    pub max_height: f32,
    pub child: E,
}

impl<E: Element> Sizer<E> {
    pub fn new(max_width: f32, max_height: f32, child: E) -> Self {
        Self {
            max_width,
            max_height,
            child,
        }
    }

    pub fn height(max_height: f32, child: E) -> Self {
        Self {
            max_width: f32::INFINITY,
            max_height,
            child,
        }
    }

    pub fn width(max_width: f32, child: E) -> Self {
        Self {
            max_width,
            max_height: f32::INFINITY,
            child,
        }
    }
}

impl<E: Element> Element for Sizer<E> {
    fn layout(&mut self, ctx: &Context, c: Constraint) -> Size {
        let inner = Constraint::new(
            self.max_width.min(c.max_width),
            self.max_height.min(c.max_height),
        );
        self.child.layout(ctx, inner)
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        self.child.show(ui, origin);
    }
}

/// Adds constant insets around a single inline child.
pub struct Padding<E: Element> {
    pub left: f32,
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
    pub child: E,
}

impl<E: Element> Padding<E> {
    pub fn new(left: f32, top: f32, right: f32, bottom: f32, child: E) -> Self {
        Self {
            left,
            top,
            right,
            bottom,
            child,
        }
    }

    pub fn all(p: f32, child: E) -> Self {
        Self {
            left: p,
            top: p,
            right: p,
            bottom: p,
            child,
        }
    }
}

impl<E: Element> Element for Padding<E> {
    fn layout(&mut self, ctx: &Context, c: Constraint) -> Size {
        let inner = Constraint::new(
            (c.max_width - self.left - self.right).max(0.0),
            (c.max_height - self.top - self.bottom).max(0.0),
        );
        let cs = self.child.layout(ctx, inner);
        Size::new(
            (cs.width + self.left + self.right).min(c.max_width),
            (cs.height + self.top + self.bottom).min(c.max_height),
        )
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        self.child
            .show(ui, Point::new(origin.x + self.left, origin.y + self.top));
    }
}

/// Single-child positional alignment — fills the constraint and places
/// the child at the alignment fraction of the leftover space.
///
/// Replaces the multi-child `Stack` from the C++ original. We don't have
/// a caller that needs multiple stacked layers yet; if/when we do (e.g.
/// for image-overlay world entries), reintroduce `Stack` with a
/// typed-tuple or boxed Vec children.
pub struct Aligned<E: Element> {
    pub alignment: Alignment,
    pub child: E,
    inner: Size,
    outer: Size,
}

impl<E: Element> Aligned<E> {
    pub fn new(alignment: Alignment, child: E) -> Self {
        Self {
            alignment,
            child,
            inner: Size::default(),
            outer: Size::default(),
        }
    }

    pub fn center(child: E) -> Self {
        Self::new(Alignment::Center, child)
    }
}

impl<E: Element> Element for Aligned<E> {
    fn layout(&mut self, ctx: &Context, c: Constraint) -> Size {
        self.outer = c.max_size();
        // Pass the same constraint to the child so it can pick its own
        // natural size up to the limit; we re-position based on the gap.
        self.inner = self.child.layout(ctx, c);
        self.outer
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        let (fx, fy) = self.alignment.fractions();
        let dx = (self.outer.width - self.inner.width) * fx;
        let dy = (self.outer.height - self.inner.height) * fy;
        self.child
            .show(ui, Point::new(origin.x + dx, origin.y + dy));
    }
}

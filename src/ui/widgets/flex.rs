//! Flexible-box container — heterogeneous children, flex-grow distribution,
//! main/cross axis size and alignment. The boxing boundary of the layout
//! engine: each `FlexItem` holds a `Box<dyn Element + 'a>` so a
//! `Vec<FlexItem<'a>>` of mixed child types is uniform.

use egui::{Context, Ui};

use crate::ui::layout::{Constraint, Element, Point, Size};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FlexDirection {
    Row,
    Column,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MainAxisSize {
    Min,
    Max,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MainAxisAlignment {
    Start,
    Center,
    End,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CrossAxisSize {
    Min,
    Max,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CrossAxisAlignment {
    Start,
    Center,
    End,
    Stretch,
}

pub struct FlexItem<'a> {
    pub child: Box<dyn Element + 'a>,
    pub flex_grow: f32,
    position: Point,
    size: Size,
}

impl<'a> FlexItem<'a> {
    pub fn new<E: Element + 'a>(child: E) -> Self {
        Self {
            child: Box::new(child),
            flex_grow: 0.0,
            position: Point::ZERO,
            size: Size::ZERO,
        }
    }

    pub fn flex<E: Element + 'a>(grow: f32, child: E) -> Self {
        Self {
            child: Box::new(child),
            flex_grow: grow,
            position: Point::ZERO,
            size: Size::ZERO,
        }
    }
}

pub struct Flex<'a> {
    pub direction: FlexDirection,
    pub main_size: MainAxisSize,
    pub main_align: MainAxisAlignment,
    pub cross_size: CrossAxisSize,
    pub cross_align: CrossAxisAlignment,
    items: Vec<FlexItem<'a>>,
}

impl<'a> Flex<'a> {
    pub fn new(direction: FlexDirection, items: Vec<FlexItem<'a>>) -> Self {
        Self {
            direction,
            main_size: MainAxisSize::Min,
            main_align: MainAxisAlignment::Start,
            cross_size: CrossAxisSize::Min,
            cross_align: CrossAxisAlignment::Start,
            items,
        }
    }

    pub fn row(items: Vec<FlexItem<'a>>) -> Self {
        Flex::new(FlexDirection::Row, items)
    }

    pub fn column(items: Vec<FlexItem<'a>>) -> Self {
        Flex::new(FlexDirection::Column, items)
    }

    pub fn main_size(mut self, v: MainAxisSize) -> Self {
        self.main_size = v;
        self
    }

    pub fn main_align(mut self, v: MainAxisAlignment) -> Self {
        self.main_align = v;
        self
    }

    pub fn cross_size(mut self, v: CrossAxisSize) -> Self {
        self.cross_size = v;
        self
    }

    pub fn cross_align(mut self, v: CrossAxisAlignment) -> Self {
        self.cross_align = v;
        self
    }
}

impl Element for Flex<'_> {
    fn layout(&mut self, ctx: &Context, c: Constraint) -> Size {
        let v = self.direction == FlexDirection::Column;
        let (max_main, max_cross) = if v {
            (c.max_height, c.max_width)
        } else {
            (c.max_width, c.max_height)
        };

        let mut sum_main = 0.0_f32;
        let mut max_cross_seen = 0.0_f32;
        let mut sum_grow = 0.0_f32;
        let mut sizes = vec![Size::ZERO; self.items.len()];

        // Pass 1: non-flexing children get unbounded main-axis constraint.
        for (i, item) in self.items.iter_mut().enumerate() {
            sum_grow += item.flex_grow;
            if item.flex_grow == 0.0 {
                let child_c = if v {
                    Constraint::new(max_cross, f32::INFINITY)
                } else {
                    Constraint::new(f32::INFINITY, max_cross)
                };
                let cs = item.child.layout(ctx, child_c);
                let (m, x) = if v {
                    (cs.height, cs.width)
                } else {
                    (cs.width, cs.height)
                };
                sum_main += m;
                max_cross_seen = max_cross_seen.max(x);
                sizes[i] = cs;
            }
        }

        // Pass 2: distribute remaining space among flex children.
        let remaining = (max_main - sum_main).max(0.0);
        for (i, item) in self.items.iter_mut().enumerate() {
            if item.flex_grow != 0.0 && sum_grow > 0.0 {
                let share = remaining * (item.flex_grow / sum_grow);
                let child_c = if v {
                    Constraint::new(max_cross, share)
                } else {
                    Constraint::new(share, max_cross)
                };
                let cs = item.child.layout(ctx, child_c);
                let (m, x) = if v {
                    (cs.height, cs.width)
                } else {
                    (cs.width, cs.height)
                };
                sum_main += m;
                max_cross_seen = max_cross_seen.max(x);
                sizes[i] = cs;
            }
        }

        // Self extent.
        let self_main = match self.main_size {
            MainAxisSize::Min => sum_main.min(max_main),
            MainAxisSize::Max => max_main,
        };
        let self_cross = match self.cross_size {
            CrossAxisSize::Min => max_cross_seen.min(max_cross),
            CrossAxisSize::Max => max_cross,
        };

        // Main axis distribution.
        let n = self.items.len() as f32;
        let leftover = (self_main - sum_main).max(0.0);
        let (mut pos, spacing) = match self.main_align {
            MainAxisAlignment::Start => (0.0, 0.0),
            MainAxisAlignment::Center => (leftover * 0.5, 0.0),
            MainAxisAlignment::End => (leftover, 0.0),
            MainAxisAlignment::SpaceBetween if n > 1.0 => (0.0, leftover / (n - 1.0)),
            MainAxisAlignment::SpaceBetween => (0.0, 0.0),
            MainAxisAlignment::SpaceAround if n > 0.0 => {
                let s = leftover / n;
                (s * 0.5, s)
            }
            MainAxisAlignment::SpaceAround => (0.0, 0.0),
            MainAxisAlignment::SpaceEvenly => {
                let s = leftover / (n + 1.0);
                (s, s)
            }
        };

        for (i, item) in self.items.iter_mut().enumerate() {
            let cs = sizes[i];
            let (m, x) = if v {
                (cs.height, cs.width)
            } else {
                (cs.width, cs.height)
            };
            let cross = match self.cross_align {
                CrossAxisAlignment::Start | CrossAxisAlignment::Stretch => 0.0,
                CrossAxisAlignment::Center => (self_cross - x) * 0.5,
                CrossAxisAlignment::End => self_cross - x,
            };
            let (px, py) = if v { (cross, pos) } else { (pos, cross) };
            item.position = Point::new(px, py);
            item.size = cs;
            pos += m + spacing;
        }

        if v {
            Size::new(self_cross, self_main)
        } else {
            Size::new(self_main, self_cross)
        }
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        for item in &mut self.items {
            item.child.show(
                ui,
                Point::new(origin.x + item.position.x, origin.y + item.position.y),
            );
        }
    }
}

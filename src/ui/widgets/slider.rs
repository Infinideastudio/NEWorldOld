//! Continuous numeric slider — hosts `egui::Slider` at our absolute rect.

use std::ops::RangeInclusive;

use egui::{Context, Ui};

use crate::ui::layout::{Constraint, Element, Point, Size, rect_at};

/// Continuous numeric slider — hosts `egui::Slider`. Value mutates in
/// place via `&mut T`; the `changed` slot is set when the user moves it.
pub struct Slider<'a, T: egui::emath::Numeric> {
    value: &'a mut T,
    range: RangeInclusive<T>,
    changed: &'a mut bool,
    size: Size,
}

impl<'a, T: egui::emath::Numeric> Slider<'a, T> {
    pub fn new(value: &'a mut T, range: RangeInclusive<T>, changed: &'a mut bool) -> Self {
        Self {
            value,
            range,
            changed,
            size: Size::ZERO,
        }
    }
}

impl<T: egui::emath::Numeric> Element for Slider<'_, T> {
    fn layout(&mut self, _ctx: &Context, c: Constraint) -> Size {
        self.size = c.into_size();
        self.size
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        let rect = rect_at(origin, self.size);
        // Force the slider track to fill our rect width — egui::Slider
        // reads `slider_width` from spacing; restore on the way out.
        let prev_w = ui.spacing().slider_width;
        ui.spacing_mut().slider_width = self.size.width;
        let resp = ui.put(
            rect,
            egui::Slider::new(self.value, self.range.clone()).show_value(false),
        );
        ui.spacing_mut().slider_width = prev_w;
        *self.changed = resp.changed();
    }
}

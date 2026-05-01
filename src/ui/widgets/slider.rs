//! Continuous numeric slider — hosts `egui::Slider` at our absolute rect.

use std::ops::RangeInclusive;

use egui::{Context, Ui};

use crate::ui::layout::{Constraint, Element, Point, Size, rect_at};

/// Continuous numeric slider — hosts `egui::Slider`. Value mutates in
/// place via `&mut T`; the `changed` slot is set when the user moves it.
pub struct Slider<'a, T: egui::emath::Numeric> {
    text: &'a str,
    value: &'a mut T,
    range: RangeInclusive<T>,
    changed: Option<&'a mut bool>,
    logarithmic: bool,
    size: Size,
}

impl<'a, T: egui::emath::Numeric> Slider<'a, T> {
    pub fn new(text: &'a str, value: &'a mut T, range: RangeInclusive<T>) -> Self {
        Self {
            text,
            value,
            range,
            changed: None,
            logarithmic: false,
            size: Size::ZERO,
        }
    }

    pub fn changed(mut self, changed: &'a mut bool) -> Self {
        self.changed = Some(changed);
        self
    }

    pub fn logarithmic(mut self, logarithmic: bool) -> Self {
        self.logarithmic = logarithmic;
        self
    }
}

impl<T: egui::emath::Numeric> Element for Slider<'_, T> {
    fn layout(&mut self, _ctx: &Context, c: Constraint) -> Size {
        self.size = c.into_size();
        self.size
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        let rect = rect_at(origin, self.size);
        let resp = ui.put(
            rect,
            egui::Slider::new(self.value, self.range.clone())
                .text(self.text)
                .logarithmic(self.logarithmic),
        );
        if let Some(changed) = &mut self.changed {
            **changed = resp.changed();
        }
    }
}

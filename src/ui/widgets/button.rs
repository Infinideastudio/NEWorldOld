//! Plain push-button — hosts `egui::Button` at our absolute rect.

use egui::{Context, Ui};

use crate::ui::layout::{Constraint, Element, Point, Size, rect_at};

/// Plain push-button. Fills the rect the parent passes; hosts an
/// `egui::Button`. The `clicked` slot is set to `true` for one frame
/// after the user releases the mouse over the button.
pub struct Button<'a> {
    text: String,
    enabled: bool,
    clicked: &'a mut bool,
    size: Size,
}

impl<'a> Button<'a> {
    pub fn new(text: impl Into<String>, clicked: &'a mut bool) -> Self {
        Self {
            text: text.into(),
            enabled: true,
            clicked,
            size: Size::ZERO,
        }
    }

    pub fn enabled(mut self, v: bool) -> Self {
        self.enabled = v;
        self
    }
}

impl Element for Button<'_> {
    fn layout(&mut self, _ctx: &Context, c: Constraint) -> Size {
        self.size = c.into_size();
        self.size
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        let rect = rect_at(origin, self.size);
        let widget = egui::Button::new(&self.text);
        let resp = if self.enabled {
            ui.put(rect, widget)
        } else {
            let mut child = ui.new_child(egui::UiBuilder::new().max_rect(rect));
            child.disable();
            child.put(rect, widget)
        };
        *self.clicked = resp.clicked();
    }
}

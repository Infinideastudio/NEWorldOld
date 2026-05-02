//! Plain push-button — hosts `egui::Button` at our absolute rect.

use egui::{Context, Ui};

use crate::ui::layout::{Constraint, Element, Point, Size, rect_at};

/// Plain push-button. Fills the rect the parent passes.
/// Attach a `&mut bool` via [`Self::clicked`] to learn whether the user
/// released the mouse over the button this frame.
pub struct Button<'a> {
    text: &'a str,
    clicked: Option<&'a mut bool>,
    enabled: bool,
    size: Size,
}

impl<'a> Button<'a> {
    pub fn new(text: &'a str) -> Self {
        Self {
            text,
            enabled: true,
            clicked: None,
            size: Size::default(),
        }
    }

    pub fn clicked(mut self, clicked: &'a mut bool) -> Self {
        self.clicked = Some(clicked);
        self
    }

    pub fn enabled(mut self, v: bool) -> Self {
        self.enabled = v;
        self
    }
}

impl Element for Button<'_> {
    fn layout(&mut self, _ctx: &Context, c: Constraint) -> Size {
        self.size = c.max_size();
        self.size
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        let rect = rect_at(origin, self.size);
        let widget = egui::Button::new(self.text);
        let mut child = ui.new_child(egui::UiBuilder::new().max_rect(rect));
        if !self.enabled {
            child.disable();
        }
        let resp = child.put(rect, widget);
        if let Some(clicked) = &mut self.clicked {
            **clicked = resp.clicked();
        }
    }
}

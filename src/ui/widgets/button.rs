//! Plain push-button — hosts `egui::Button` at our absolute rect.

use egui::{Context, Ui};

use crate::ui::layout::{Constraint, Element, Point, Size, rect_at};

/// Plain push-button. Fills the rect the parent passes; hosts an
/// `egui::Button`. Attach a `&mut bool` via [`Self::clicked`] to learn
/// whether the user released the mouse over the button this frame —
/// callers that don't care about the click can omit it.
pub struct Button<'a> {
    text: &'a str,
    enabled: bool,
    clicked: Option<&'a mut bool>,
    size: Size,
}

impl<'a> Button<'a> {
    pub fn new(text: &'a str) -> Self {
        Self {
            text,
            enabled: true,
            clicked: None,
            size: Size::ZERO,
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
        self.size = c.into_size();
        self.size
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        let rect = rect_at(origin, self.size);
        let widget = egui::Button::new(self.text);
        let resp = if self.enabled {
            ui.put(rect, widget)
        } else {
            let mut child = ui.new_child(egui::UiBuilder::new().max_rect(rect));
            child.disable();
            child.put(rect, widget)
        };
        if let Some(clicked) = &mut self.clicked {
            **clicked = resp.clicked();
        }
    }
}

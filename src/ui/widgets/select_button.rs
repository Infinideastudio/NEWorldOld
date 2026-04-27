//! Selectable (toggle-style) button — hosts `egui::Button::selectable`.
//! Used for list-row entries where the same widget reports both single
//! and double clicks via separate output flags.

use egui::{Context, Ui};

use crate::ui::layout::{Constraint, Element, Point, Size, rect_at};

/// Output slot for [`SelectButton`]. Single and double clicks come through
/// the same widget so we keep them together.
#[derive(Debug, Clone, Copy, Default)]
pub struct SelectButtonOutput {
    pub clicked: bool,
    pub double_clicked: bool,
}

pub struct SelectButton<'a> {
    text: String,
    selected: bool,
    out: &'a mut SelectButtonOutput,
    size: Size,
}

impl<'a> SelectButton<'a> {
    pub fn new(text: impl Into<String>, selected: bool, out: &'a mut SelectButtonOutput) -> Self {
        Self {
            text: text.into(),
            selected,
            out,
            size: Size::ZERO,
        }
    }
}

impl Element for SelectButton<'_> {
    fn layout(&mut self, _ctx: &Context, c: Constraint) -> Size {
        self.size = c.into_size();
        self.size
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        let rect = rect_at(origin, self.size);
        let resp = ui.put(rect, egui::Button::selectable(self.selected, &self.text));
        self.out.clicked = resp.clicked();
        self.out.double_clicked = resp.double_clicked();
    }
}

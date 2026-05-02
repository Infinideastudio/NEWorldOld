//! Selectable (toggle-style) button — hosts `egui::Button::selectable`.
//! Used for list-row entries where the same widget reports both single
//! and double clicks via separate output flags.

use egui::{Context, Ui};

use crate::ui::layout::{Constraint, Element, Point, Size, rect_at};

/// Selectable button — list-row entry. Reports single and double clicks
/// independently; attach `&mut bool` slots only for whichever events
/// the caller cares about via [`Self::clicked`] / [`Self::double_clicked`].
pub struct SelectButton<'a> {
    text: &'a str,
    selected: bool,
    clicked: Option<&'a mut bool>,
    double_clicked: Option<&'a mut bool>,
    size: Size,
}

impl<'a> SelectButton<'a> {
    pub fn new(text: &'a str, selected: bool) -> Self {
        Self {
            text,
            selected,
            clicked: None,
            double_clicked: None,
            size: Size::default(),
        }
    }

    pub fn clicked(mut self, clicked: &'a mut bool) -> Self {
        self.clicked = Some(clicked);
        self
    }

    pub fn double_clicked(mut self, double_clicked: &'a mut bool) -> Self {
        self.double_clicked = Some(double_clicked);
        self
    }
}

impl Element for SelectButton<'_> {
    fn layout(&mut self, _ctx: &Context, c: Constraint) -> Size {
        self.size = c.max_size();
        self.size
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        let rect = rect_at(origin, self.size);
        // Always paint the full button frame (background + border) — the
        // C++ `Button::selectable` factory paints text-only in the
        // unselected state, which makes list rows hard to spot against
        // the menu background. `Button::new(...).selected(bool)` keeps
        // the chrome and just tints the fill when selected.
        let widget = egui::Button::new(self.text).selected(self.selected);
        let resp = ui.put(rect, widget);
        if let Some(clicked) = &mut self.clicked {
            **clicked = resp.clicked();
        }
        if let Some(double_clicked) = &mut self.double_clicked {
            **double_clicked = resp.double_clicked();
        }
    }
}

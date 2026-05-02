//! Single-line text input — hosts `egui::TextEdit::singleline`. Mutates
//! the supplied `&mut String` in place; per-frame events (per-keystroke
//! `changed`, on-Enter `submitted`) are reported via optional
//! builder-attached `&mut bool` slots.

use egui::{Context, Ui};

use crate::ui::layout::{Constraint, Element, Point, Size, rect_at};

pub struct TextEdit<'a> {
    text: &'a mut String,
    hint: String,
    changed: Option<&'a mut bool>,
    submitted: Option<&'a mut bool>,
    size: Size,
}

impl<'a> TextEdit<'a> {
    pub fn new(text: &'a mut String) -> Self {
        Self {
            text,
            hint: String::new(),
            changed: None,
            submitted: None,
            size: Size::default(),
        }
    }

    pub fn hint(mut self, hint: impl Into<String>) -> Self {
        self.hint = hint.into();
        self
    }

    pub fn changed(mut self, changed: &'a mut bool) -> Self {
        self.changed = Some(changed);
        self
    }

    pub fn submitted(mut self, submitted: &'a mut bool) -> Self {
        self.submitted = Some(submitted);
        self
    }
}

impl Element for TextEdit<'_> {
    fn layout(&mut self, _ctx: &Context, c: Constraint) -> Size {
        self.size = c.max_size();
        self.size
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        let rect = rect_at(origin, self.size);
        let resp = ui.put(
            rect,
            egui::TextEdit::singleline(self.text)
                .hint_text(&self.hint)
                .desired_width(f32::INFINITY),
        );
        if let Some(changed) = &mut self.changed {
            **changed = resp.changed();
        }
        if let Some(submitted) = &mut self.submitted {
            **submitted = resp.lost_focus() && ui.input(|i| i.key_pressed(egui::Key::Enter));
        }
    }
}

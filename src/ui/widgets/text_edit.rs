//! Single-line text input — hosts `egui::TextEdit::singleline`. Mutates
//! the supplied `&mut String` in place; the output slot reports per-frame
//! events.

use egui::{Context, Ui};

use crate::ui::layout::{Constraint, Element, Point, Size, rect_at};

/// Output slot for [`TextEdit`]. `changed` fires per keystroke; `submitted`
/// fires on Enter (or the platform's submission gesture).
#[derive(Copy, Clone, Default, Debug)]
pub struct TextEditOutput {
    pub changed: bool,
    pub submitted: bool,
}

pub struct TextEdit<'a> {
    text: &'a mut String,
    hint: String,
    out: &'a mut TextEditOutput,
    size: Size,
}

impl<'a> TextEdit<'a> {
    pub fn singleline(text: &'a mut String, out: &'a mut TextEditOutput) -> Self {
        Self {
            text,
            hint: String::new(),
            out,
            size: Size::ZERO,
        }
    }

    pub fn hint(mut self, hint: impl Into<String>) -> Self {
        self.hint = hint.into();
        self
    }
}

impl Element for TextEdit<'_> {
    fn layout(&mut self, _ctx: &Context, c: Constraint) -> Size {
        self.size = c.into_size();
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
        self.out.changed = resp.changed();
        self.out.submitted = resp.lost_focus() && ui.input(|i| i.key_pressed(egui::Key::Enter));
    }
}

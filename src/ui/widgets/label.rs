//! Label — a leaf element drawn directly via `egui::Painter` (not via
//! `ui.put`, so we don't pay for egui's widget allocation machinery for
//! something that's purely visual).

use std::sync::Arc;

use egui::epaint::text::Galley;
use egui::{Color32, Context, FontId, Pos2, Ui};

use crate::ui::layout::{Constraint, Element, Point, Size, default_body_font};

pub struct Label<'a> {
    text: &'a str,
    font_id: Option<FontId>,
    color: Option<Color32>,
    galley: Option<Arc<Galley>>,
}

impl<'a> Label<'a> {
    pub fn new(text: &'a str) -> Self {
        Self {
            text,
            font_id: None,
            color: None,
            galley: None,
        }
    }

    pub fn font(mut self, id: FontId) -> Self {
        self.font_id = Some(id);
        self
    }

    pub fn color(mut self, c: Color32) -> Self {
        self.color = Some(c);
        self
    }
}

impl<'a> Element for Label<'a> {
    fn layout(&mut self, ctx: &Context, c: Constraint) -> Size {
        let font = self
            .font_id
            .clone()
            .unwrap_or_else(|| default_body_font(ctx));
        let color = self
            .color
            .unwrap_or_else(|| ctx.global_style().visuals.text_color());
        // `layout_no_wrap` is a `&mut` method on FontsView (it memoizes
        // galleys), so we go through `fonts_mut`. The returned Arc is
        // cheap to keep around for the show pass.
        let galley = ctx.fonts_mut(|f| f.layout_no_wrap(self.text.into(), font, color));
        let s = galley.size();
        let size = Size::new(s.x.min(c.max_width), s.y.min(c.max_height));
        self.galley = Some(galley);
        size
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        if let Some(g) = &self.galley {
            let color = self
                .color
                .unwrap_or_else(|| ui.style().visuals.text_color());
            ui.painter()
                .galley(Pos2::new(origin.x, origin.y), g.clone(), color);
        }
    }
}

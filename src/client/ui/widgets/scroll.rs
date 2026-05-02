//! Vertical / horizontal scroll viewport — layout-aware so the child sees
//! infinite constraint along the scroll axis and can report its natural
//! extent. Persists scroll offset across frames in `egui::Memory`; the
//! tree itself is rebuilt every frame.

use egui::{Color32, Context, Pos2, Rect, Ui};

use crate::ui::layout::{Constraint, Element, Point, Size, rect_at};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ScrollDirection {
    Vertical,
    Horizontal,
}

pub struct ScrollView<E: Element> {
    pub direction: ScrollDirection,
    pub child: E,
    pub id: egui::Id,
    outer: Size,
    inner: Size,
}

impl<E: Element> ScrollView<E> {
    pub fn vertical(id: egui::Id, child: E) -> Self {
        Self {
            direction: ScrollDirection::Vertical,
            child,
            id,
            outer: Size::default(),
            inner: Size::default(),
        }
    }
}

#[derive(Debug, Clone, Copy, Default)]
struct ScrollState {
    offset: f32,
}

impl<E: Element> Element for ScrollView<E> {
    fn layout(&mut self, ctx: &Context, c: Constraint) -> Size {
        self.outer = c.max_size();
        let inner_c = match self.direction {
            ScrollDirection::Vertical => Constraint::new(self.outer.width, f32::INFINITY),
            ScrollDirection::Horizontal => Constraint::new(f32::INFINITY, self.outer.height),
        };
        self.inner = self.child.layout(ctx, inner_c);
        self.outer
    }

    fn show(&mut self, ui: &mut Ui, origin: Point) {
        let outer_rect = rect_at(origin, self.outer);
        let max_scroll = match self.direction {
            ScrollDirection::Vertical => (self.inner.height - self.outer.height).max(0.0),
            ScrollDirection::Horizontal => (self.inner.width - self.outer.width).max(0.0),
        };

        let mut state: ScrollState = ui.ctx().data(|d| d.get_temp(self.id).unwrap_or_default());

        let pointer_in = ui.rect_contains_pointer(outer_rect);
        if pointer_in {
            let dy = ui.input(|i| match self.direction {
                ScrollDirection::Vertical => i.smooth_scroll_delta.y,
                ScrollDirection::Horizontal => i.smooth_scroll_delta.x,
            });
            state.offset = (state.offset - dy).clamp(0.0, max_scroll);
        } else {
            state.offset = state.offset.clamp(0.0, max_scroll);
        }
        ui.ctx().data_mut(|d| d.insert_temp(self.id, state));

        // Build a child Ui clipped to the outer rect. Any egui widget
        // hosted inside (Button, Slider, …) inherits this clip rect.
        let mut child_ui = ui.new_child(
            egui::UiBuilder::new()
                .max_rect(outer_rect)
                .layout(egui::Layout::left_to_right(egui::Align::Min)),
        );
        child_ui.set_clip_rect(outer_rect.intersect(ui.clip_rect()));

        let child_origin = match self.direction {
            ScrollDirection::Vertical => Point::new(origin.x, origin.y - state.offset),
            ScrollDirection::Horizontal => Point::new(origin.x - state.offset, origin.y),
        };

        self.child.show(&mut child_ui, child_origin);

        if max_scroll > 0.0 {
            self.draw_scrollbar(ui, outer_rect, state.offset, max_scroll);
        }
    }
}

impl<E: Element> ScrollView<E> {
    fn draw_scrollbar(&self, ui: &mut Ui, rect: Rect, offset: f32, max_scroll: f32) {
        const BAR: f32 = 4.0;
        let painter = ui.painter();
        let track_color = Color32::from_black_alpha(40);
        let thumb_color = Color32::from_white_alpha(80);

        match self.direction {
            ScrollDirection::Vertical => {
                let track = Rect::from_min_max(
                    Pos2::new(rect.max.x - BAR, rect.min.y),
                    Pos2::new(rect.max.x, rect.max.y),
                );
                let visible_frac = (self.outer.height / self.inner.height).clamp(0.05, 1.0);
                let thumb_h = (track.height() * visible_frac).max(20.0);
                let track_travel = track.height() - thumb_h;
                let thumb_y = track.min.y + track_travel * (offset / max_scroll);
                let thumb = Rect::from_min_max(
                    Pos2::new(track.min.x, thumb_y),
                    Pos2::new(track.max.x, thumb_y + thumb_h),
                );
                painter.rect_filled(track, 0.0, track_color);
                painter.rect_filled(thumb, 2.0, thumb_color);
            }
            ScrollDirection::Horizontal => {
                let track = Rect::from_min_max(
                    Pos2::new(rect.min.x, rect.max.y - BAR),
                    Pos2::new(rect.max.x, rect.max.y),
                );
                let visible_frac = (self.outer.width / self.inner.width).clamp(0.05, 1.0);
                let thumb_w = (track.width() * visible_frac).max(20.0);
                let track_travel = track.width() - thumb_w;
                let thumb_x = track.min.x + track_travel * (offset / max_scroll);
                let thumb = Rect::from_min_max(
                    Pos2::new(thumb_x, track.min.y),
                    Pos2::new(thumb_x + thumb_w, track.max.y),
                );
                painter.rect_filled(track, 0.0, track_color);
                painter.rect_filled(thumb, 2.0, thumb_color);
            }
        }
    }
}

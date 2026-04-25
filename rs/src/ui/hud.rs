//! In-game HUD overlay (E4) plus selection outline + chat dispatch (F2/F3).
//!
//! Provides a [`Hud`] struct that the game screen composes. Renders:
//! * A static crosshair at screen center.
//! * A debug panel (F3-style) showing position, orientation, FPS, chunk count.
//! * A chat input bar at the bottom of the screen, with Enter / Tab handling.
//! * The currently-selected block as a 12-line wireframe outline projected to
//!   screen space via the supplied `view_proj`.
//! * Recent chat history above the chat bar (auto-decay after 5 s, or always
//!   visible while the chat bar is open).

use cgmath::Matrix4;

use crate::game::Hit;
use crate::math::Vec3i;

/// Per-frame data the HUD reads. Bundled to keep [`Hud::render`] under the
/// clippy `too_many_arguments` threshold and to make it obvious which fields
/// are inputs vs. owned `Hud` state.
pub struct HudFrame<'a> {
    pub camera_pos: [f64; 3],
    pub yaw: f64,
    pub pitch: f64,
    pub fps: f32,
    pub chunk_count: usize,
    pub selected: Option<Hit>,
    pub view_proj: Matrix4<f32>,
    pub chat_history: &'a [&'a str],
}

/// 12 cube edges; corners are addressed by the dx/dy/dz bit scheme used
/// inside [`Hud::render_selection_outline`] (bit 0 = x, bit 1 = y, bit 2 = z).
const SELECTION_EDGES: [(usize, usize); 12] = [
    (0, 1),
    (2, 3),
    (4, 5),
    (6, 7),
    (0, 2),
    (1, 3),
    (4, 6),
    (5, 7),
    (0, 4),
    (1, 5),
    (2, 6),
    (3, 7),
];

/// In-game HUD state.
#[derive(Default)]
pub struct Hud {
    /// Chat text input buffer (when chat is open).
    pub chat_input: String,
    /// Whether the chat input is focused.
    pub chat_open: bool,
    /// Whether the debug panel (F3-style) is visible.
    pub debug_open: bool,
    /// Whether inventory is open.
    pub inventory_open: bool,
    /// Lines submitted via Enter this frame; the game screen drains these
    /// after `render` and forwards them to the command registry.
    submitted: Vec<String>,
}

impl Hud {
    /// Handle toggle keys. Call from the game screen's `ui` method before
    /// building any UI, so key presses don't leak into text inputs.
    pub fn handle_input(&mut self, ctx: &egui::Context) {
        // Don't let toggle keys fire while the chat bar is open and capturing
        // text — otherwise typing "T" or "/" would close chat.
        if self.chat_open {
            return;
        }
        ctx.input(|i| {
            if i.key_pressed(egui::Key::F3) {
                self.debug_open = !self.debug_open;
            }
            if i.key_pressed(egui::Key::T) || i.key_pressed(egui::Key::Slash) {
                self.chat_open = true;
                self.chat_input.clear();
                // The Slash key event will also be picked up by the text edit
                // on the same frame (request_focus runs after this), so we
                // don't push a leading `/` manually — that would yield `//`.
            }
            if i.key_pressed(egui::Key::E) {
                self.inventory_open = !self.inventory_open;
            }
        });
    }

    /// Render the full HUD. Call from the game screen's `ui` method.
    ///
    /// `selected` and `view_proj` come from the [`crate::game::Game`] tick:
    /// when `selected` is `Some`, we draw a wireframe outline of the block.
    /// `chat_history` is the auto-decayed list of recent chat lines (most
    /// recent last).
    pub fn render(&mut self, ctx: &egui::Context, frame: &HudFrame<'_>) {
        Self::render_crosshair(ctx);
        self.render_debug_panel(
            ctx,
            frame.camera_pos,
            frame.yaw,
            frame.pitch,
            frame.fps,
            frame.chunk_count,
        );
        if let Some(hit) = frame.selected {
            Self::render_selection_outline(ctx, hit.coord, frame.view_proj);
        }
        Self::render_chat_history(ctx, frame.chat_history);

        if self.chat_open {
            self.render_chat(ctx);
        }
    }

    /// Drain the chat lines submitted this frame. Called by the game screen
    /// after `render` so it can forward them to the [`crate::game::Game`]
    /// dispatcher.
    pub fn drain_submitted(&mut self) -> Vec<String> {
        std::mem::take(&mut self.submitted)
    }

    /// Replace the chat input with `replacement`, used by Tab autocomplete.
    pub fn set_chat_input(&mut self, replacement: String) {
        self.chat_input = replacement;
    }

    /// Draw a small crosshair at the center of the screen.
    fn render_crosshair(ctx: &egui::Context) {
        egui::Area::new("crosshair".into())
            .fixed_pos(ctx.content_rect().center())
            .movable(false)
            .interactable(false)
            .show(ctx, |ui| {
                ui.add(
                    egui::Label::new(
                        egui::RichText::new("+")
                            .size(24.0)
                            .color(egui::Color32::from_rgba_unmultiplied(255, 255, 255, 200)),
                    ),
                );
            });
    }

    /// F3-style debug overlay showing position, orientation, FPS, chunk count.
    fn render_debug_panel(
        &self,
        ctx: &egui::Context,
        camera_pos: [f64; 3],
        yaw: f64,
        pitch: f64,
        fps: f32,
        chunk_count: usize,
    ) {
        if !self.debug_open {
            return;
        }

        egui::Window::new("Debug")
            .collapsible(false)
            .resizable(false)
            .default_pos([4.0, 4.0])
            .show(ctx, |ui| {
                ui.label(format!(
                    "Position: ({:.2}, {:.2}, {:.2})",
                    camera_pos[0], camera_pos[1], camera_pos[2]
                ));
                ui.label(format!("Yaw: {yaw:.2}  Pitch: {pitch:.2}"));
                ui.label(format!("FPS: {fps:.0}"));
                ui.label(format!("Chunks: {chunk_count}"));
            });
    }

    /// Project the 8 corners of a unit cube at `coord` through `view_proj`
    /// and draw 12 line segments via egui. Skips when any corner is behind
    /// the camera (`w <= 0`).
    #[allow(clippy::many_single_char_names)] // x/y/z/w mirror cgmath::Matrix4 col names
    fn render_selection_outline(ctx: &egui::Context, coord: Vec3i, view_proj: Matrix4<f32>) {
        let rect = ctx.content_rect();
        let width = rect.width();
        let height = rect.height();
        let origin = rect.left_top();

        let base = [coord.x as f32, coord.y as f32, coord.z as f32];
        let mut clip = [(0.0_f32, 0.0_f32, 0.0_f32, 0.0_f32); 8];
        for (idx, slot) in clip.iter_mut().enumerate() {
            let dx = ((idx & 1) != 0) as i32 as f32;
            let dy = ((idx & 2) != 0) as i32 as f32;
            let dz = ((idx & 4) != 0) as i32 as f32;
            let x = base[0] + dx;
            let y = base[1] + dy;
            let z = base[2] + dz;
            // Manual mat4 * vec4 — cgmath's Matrix4 stores columns in `.x`,
            // `.y`, `.z`, `.w`; the `.x.x` etc. fields are individual scalars.
            let cx = view_proj.x.x * x + view_proj.y.x * y + view_proj.z.x * z + view_proj.w.x;
            let cy = view_proj.x.y * x + view_proj.y.y * y + view_proj.z.y * z + view_proj.w.y;
            let cz = view_proj.x.z * x + view_proj.y.z * y + view_proj.z.z * z + view_proj.w.z;
            let cw = view_proj.x.w * x + view_proj.y.w * y + view_proj.z.w * z + view_proj.w.w;
            *slot = (cx, cy, cz, cw);
        }

        if clip.iter().any(|(_, _, _, cw)| *cw <= 0.0) {
            return;
        }

        let mut screen = [egui::pos2(0.0, 0.0); 8];
        for (idx, (cx, cy, _, cw)) in clip.iter().enumerate() {
            let ndc_x = cx / cw;
            let ndc_y = cy / cw;
            let sx = origin.x + (ndc_x + 1.0) * 0.5 * width;
            let sy = origin.y + (1.0 - (ndc_y + 1.0) * 0.5) * height;
            screen[idx] = egui::pos2(sx, sy);
        }

        let painter = ctx.layer_painter(egui::LayerId::new(
            egui::Order::Foreground,
            egui::Id::new("selection_outline"),
        ));
        let stroke = egui::Stroke::new(1.5, egui::Color32::from_gray(220));
        for (a, b) in SELECTION_EDGES {
            painter.line_segment([screen[a], screen[b]], stroke);
        }
    }

    /// Recent chat lines, drawn as a translucent panel above the chat bar.
    fn render_chat_history(ctx: &egui::Context, lines: &[&str]) {
        if lines.is_empty() {
            return;
        }
        egui::Area::new("chat_history".into())
            .anchor(egui::Align2::LEFT_BOTTOM, egui::vec2(8.0, -32.0))
            .interactable(false)
            .show(ctx, |ui| {
                let frame = egui::Frame::NONE
                    .inner_margin(4.0)
                    .fill(egui::Color32::from_black_alpha(120));
                frame.show(ui, |ui| {
                    for line in lines {
                        ui.colored_label(egui::Color32::from_gray(230), *line);
                    }
                });
            });
    }

    /// Chat input bar pinned to the bottom of the screen. Handles Enter
    /// (submit) and Escape (close).
    #[allow(deprecated)] // Panel::bottom::show
    fn render_chat(&mut self, ctx: &egui::Context) {
        let mut close = false;
        let mut submit_now: Option<String> = None;
        egui::Panel::bottom("chat_bar").show(ctx, |ui| {
            ui.horizontal(|ui| {
                ui.label(">");
                let response = ui.text_edit_singleline(&mut self.chat_input);
                response.request_focus();
            });
        });
        ctx.input(|i| {
            if i.key_pressed(egui::Key::Escape) {
                close = true;
            }
            if i.key_pressed(egui::Key::Enter) {
                let line = std::mem::take(&mut self.chat_input);
                submit_now = Some(line);
                close = true;
            }
        });
        if let Some(line) = submit_now
            && !line.is_empty()
        {
            self.submitted.push(line);
        }
        if close {
            self.chat_input.clear();
            self.chat_open = false;
        }
    }
}

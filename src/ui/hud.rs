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

/// Per-frame data the HUD reads. Bundled to keep [`Hud::render`] under the
/// clippy `too_many_arguments` threshold and to make it obvious which fields
/// are inputs vs. owned `Hud` state.
pub struct HudFrame<'a> {
    pub camera_pos: [f64; 3],
    pub yaw: f64,
    pub pitch: f64,
    pub fps: f32,
    pub chunk_count: usize,
    pub chat_history: &'a [&'a str],
}

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
    /// The selection wireframe is *not* drawn here — that's a real 3-D
    /// pass owned by [`crate::render::SelectionPipeline`], composited into
    /// the world render pass before egui so UI elements always sit on top.
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

//! In-game HUD overlay (E4).
//!
//! Provides a [`Hud`] struct that the game screen composes. Renders:
//! * A static crosshair at screen center.
//! * A debug panel (F3-style) showing position, orientation, FPS, chunk count.
//! * A chat input bar at the bottom of the screen.

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
}

impl Hud {
    /// Handle toggle keys. Call from the game screen's `ui` method before
    /// building any UI, so key presses don't leak into text inputs.
    pub fn handle_input(&mut self, ctx: &egui::Context) {
        ctx.input(|i| {
            if i.key_pressed(egui::Key::F3) {
                self.debug_open = !self.debug_open;
            }
            if i.key_pressed(egui::Key::T) || i.key_pressed(egui::Key::Slash) {
                self.chat_open = !self.chat_open;
                if self.chat_open {
                    self.chat_input.clear();
                }
            }
            if i.key_pressed(egui::Key::E) {
                self.inventory_open = !self.inventory_open;
            }
        });
    }

    /// Render the full HUD. Call from the game screen's `ui` method.
    pub fn render(
        &mut self,
        ctx: &egui::Context,
        camera_pos: [f64; 3],
        yaw: f64,
        pitch: f64,
        fps: f32,
        chunk_count: usize,
    ) {
        Self::render_crosshair(ctx);
        self.render_debug_panel(ctx, camera_pos, yaw, pitch, fps, chunk_count);

        if self.chat_open {
            self.render_chat(ctx);
        }
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

    /// Chat input bar pinned to the bottom of the screen.
    #[allow(deprecated)] // Panel::bottom::show
    fn render_chat(&mut self, ctx: &egui::Context) {
        egui::Panel::bottom("chat_bar").show(ctx, |ui| {
            ui.horizontal(|ui| {
                ui.label(">");
                let response = ui.text_edit_singleline(&mut self.chat_input);
                if self.chat_open {
                    response.request_focus();
                }
            });
        });
    }
}

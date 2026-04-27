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
/// are inputs vs. owned `Hud` state. Field set mirrors C++ `neworld.ixx`'s
/// F3 debug panel (~line 1115).
pub struct HudFrame<'a> {
    pub camera_pos: [f64; 3],
    pub yaw: f64,
    pub pitch: f64,
    pub fps: f32,
    pub ups: f32,
    pub chunk_count: usize,
    pub rendered_chunks: usize,
    pub unloaded_chunks: u32,
    pub meshed_chunks: usize,
    pub creative: bool,
    pub cross_wall: bool,
    pub grounded: bool,
    pub near_wall: bool,
    pub in_water: bool,
    pub game_time: u32,
    pub updated_blocks: u32,
    pub pending_block_updates: usize,
    pub advanced_render: bool,
    pub show_shadow_map: bool,
    pub backend: &'a str,
    pub chat_history: &'a [&'a str],
    /// Pre-formatted "Selected: …" line for the F3 debug panel — coord
    /// + raw id + registry name of the cell the player is currently
    /// aiming at, or `None` if the raycast missed. Set by the app each
    /// frame from [`crate::game::Game::selected_block_info`].
    pub selected_block: Option<&'a str>,
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
    /// Set true when chat is opened with a pre-filled prefix (slash key).
    /// Consumed on the first `render_chat` after — moves the textedit
    /// caret to the end of the buffer so the user types *after* the
    /// prefix instead of inserting in front of it. egui's default
    /// caret-on-focus is position 0, which lands before the slash.
    pending_cursor_at_end: bool,
    /// Lines submitted via Enter this frame; the game screen drains these
    /// after `render` and forwards them to the command registry.
    submitted: Vec<String>,
}

impl Hud {
    /// Handle toggle keys. Call from the game screen's `ui` method before
    /// building any UI, so key presses don't leak into text inputs.
    pub fn handle_input(&mut self, ctx: &egui::Context) {
        // Don't let toggle keys fire while the chat bar is open — its own
        // Enter handler would race the open-chat handler below.
        if self.chat_open {
            return;
        }
        // F3 / E aren't text-input keys, so non-consuming `key_pressed` is
        // fine. Enter and Slash open chat and *must* be consumed —
        // otherwise the same key event reaches the freshly-focused
        // textedit on the next frame and types itself in (Slash would
        // give `//`, Enter would submit empty + close chat).
        let press_f3 = ctx.input(|i| i.key_pressed(egui::Key::F3));
        let press_e = ctx.input(|i| i.key_pressed(egui::Key::E));
        let press_enter = ctx.input_mut(|i| i.consume_key(egui::Modifiers::NONE, egui::Key::Enter));
        let press_slash = ctx.input_mut(|i| i.consume_key(egui::Modifiers::NONE, egui::Key::Slash));

        if press_f3 {
            self.debug_open = !self.debug_open;
        }
        if press_e {
            self.inventory_open = !self.inventory_open;
        }
        if press_enter {
            self.chat_open = true;
            self.chat_input.clear();
        } else if press_slash {
            // Pre-fill with `/` so command typing skips the leading char.
            // Slash + non-empty buffer can't both happen on the same frame
            // because Enter took priority above.
            self.chat_open = true;
            self.chat_input = "/".to_string();
            self.pending_cursor_at_end = true;
        }
    }

    /// Render the full HUD. Call from the game screen's `ui` method.
    ///
    /// The selection wireframe is *not* drawn here — that's a real 3-D
    /// pass owned by [`crate::render::SelectionPipeline`], composited into
    /// the world render pass before egui so UI elements always sit on top.
    pub fn render(&mut self, ctx: &egui::Context, frame: &HudFrame<'_>) {
        // Crosshair is drawn by `crate::render::CrosshairPipeline` in the
        // world pass — see `Game::record_world_pass`. No egui overlay
        // here so the inverted-color `+` stays above world overlays
        // without z-fighting against UI panels.
        self.render_debug_panel(ctx, frame);
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

    /// F3-style debug overlay. Layout mirrors the C++ build's `neworld.ixx`
    /// debug panel (~line 1115): version + backend, fps/ups, game mode,
    /// shadow-view toggle, cross-wall, position, heading/pitch, body
    /// flags, world clock, chunk counters, block-update tally. Drawn as
    /// a top-left `egui::Area` (no window chrome, no drag) so it stays
    /// pinned regardless of viewport size.
    fn render_debug_panel(&self, ctx: &egui::Context, f: &HudFrame<'_>) {
        if !self.debug_open {
            return;
        }
        let version = env!("CARGO_PKG_VERSION");
        let mode_str = if f.creative { "creative" } else { "survival" };
        let bool_str = |b: bool| if b { "true" } else { "false" };

        // Heading / pitch in degrees, matching C++. Yaw maps to heading.
        let heading_deg = f.yaw.to_degrees();
        let pitch_deg = f.pitch.to_degrees();

        // World clock — same arithmetic as C++ `neworld.ixx:1137-1140`:
        // 30 ticks/sec game day = 24 in-game hours. h:m:s + raw tick.
        let game_time = f.game_time;
        let h = (game_time / 30 / 60) % 24;
        let m = (game_time / 30) % 60;
        let s = (game_time % 30) * 2;

        egui::Area::new("debug_panel".into())
            .anchor(egui::Align2::LEFT_TOP, egui::vec2(8.0, 8.0))
            .interactable(false)
            .show(ctx, |ui| {
                let frame = egui::Frame::NONE
                    .inner_margin(6.0)
                    .fill(egui::Color32::from_black_alpha(140));
                frame.show(ui, |ui| {
                    let line = |ui: &mut egui::Ui, s: String| {
                        let rich = egui::RichText::new(s).color(egui::Color32::from_gray(230));
                        let label = egui::Label::new(rich).wrap_mode(egui::TextWrapMode::Extend);
                        ui.add(label);
                    };
                    line(ui, format!("v{version} [{}]", f.backend));
                    line(ui, format!("{:.0} fps, {:.0} ups", f.fps, f.ups));
                    line(ui, format!("Game mode: {mode_str} mode"));
                    if f.advanced_render {
                        line(ui, format!("shadow view: {}", bool_str(f.show_shadow_map)));
                    }
                    line(ui, format!("cross wall: {}", bool_str(f.cross_wall)));
                    line(
                        ui,
                        format!(
                            "x: {:.3} y: {:.3} z: {:.3}",
                            f.camera_pos[0], f.camera_pos[1], f.camera_pos[2]
                        ),
                    );
                    line(
                        ui,
                        format!("heading: {heading_deg:.3}, pitch: {pitch_deg:.3}"),
                    );
                    line(
                        ui,
                        format!(
                            "grounded: {}, near wall: {}, in water: {}",
                            bool_str(f.grounded),
                            bool_str(f.near_wall),
                            bool_str(f.in_water)
                        ),
                    );
                    line(ui, format!("time: {h:02}:{m:02}:{s:02} ({game_time})"));
                    line(
                        ui,
                        format!(
                            "chunks {} loaded, {} rendered, {} unloaded, {} meshed",
                            f.chunk_count, f.rendered_chunks, f.unloaded_chunks, f.meshed_chunks
                        ),
                    );
                    line(
                        ui,
                        format!(
                            "{}/{} blocks updated",
                            f.updated_blocks, f.pending_block_updates
                        ),
                    );
                    line(
                        ui,
                        match f.selected_block {
                            Some(s) => format!("Selected: {s}"),
                            None => "Selected: none".to_owned(),
                        },
                    );
                });
            });
    }

    /// Recent chat lines, drawn as a translucent panel above the chat bar.
    /// Wrap mode is forced to `Extend` so the panel sizes to the *longest*
    /// line. egui's default heuristic measures each label against the
    /// parent's available width and, with no explicit override, ended up
    /// wrapping all lines to the shortest one's footprint.
    fn render_chat_history(ctx: &egui::Context, lines: &[&str]) {
        if lines.is_empty() {
            return;
        }
        let combined = lines.join("\n");
        egui::Area::new("chat_history".into())
            .anchor(egui::Align2::LEFT_BOTTOM, egui::vec2(8.0, -32.0))
            .interactable(false)
            .show(ctx, |ui| {
                let frame = egui::Frame::NONE
                    .inner_margin(4.0)
                    .fill(egui::Color32::from_black_alpha(120));
                frame.show(ui, |ui| {
                    let rich = egui::RichText::new(combined).color(egui::Color32::from_gray(230));
                    ui.add(egui::Label::new(rich).wrap_mode(egui::TextWrapMode::Extend));
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
                if self.pending_cursor_at_end {
                    // Move the caret past the pre-filled prefix on the
                    // first frame after slash-opens-chat. Loading the
                    // textedit's state by widget id, mutating its cursor,
                    // and storing it back is the canonical egui pattern
                    // for programmatic caret control.
                    let id = response.id;
                    let mut state = egui::TextEdit::load_state(ctx, id).unwrap_or_default();
                    let pos = self.chat_input.chars().count();
                    let ccursor = egui::text::CCursor::new(pos);
                    state
                        .cursor
                        .set_char_range(Some(egui::text::CCursorRange::one(ccursor)));
                    state.store(ctx, id);
                    self.pending_cursor_at_end = false;
                }
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

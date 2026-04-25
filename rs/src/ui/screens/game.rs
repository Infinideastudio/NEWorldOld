//! In-game screen — HUD overlay and pause menu.
//!
//! The actual 3D world render pass is handled by `app.rs`, not by this
//! screen. This screen provides the HUD top bar, crosshair, debug panel,
//! chat bar, inventory window, and the pause menu (Escape).

use egui::Context;

use super::super::screen::{Screen, Transition};
use super::super::hud::Hud;
use super::super::inventory::Inventory;
use super::{OptionsScreen, TitleScreen};

/// The in-game screen shown during gameplay.
///
/// Composes [`Hud`] (crosshair, debug, chat) and [`Inventory`] (item grid).
pub struct GameScreen {
    pub paused: bool,
    pub fps: f32,
    pub camera_pos: [f64; 3],
    pub yaw: f64,
    pub pitch: f64,
    pub chunk_count: usize,
    pub hud: Hud,
    pub inventory: Inventory,
}

impl Default for GameScreen {
    fn default() -> Self {
        Self {
            paused: false,
            fps: 0.0,
            camera_pos: [0.0; 3],
            yaw: 0.0,
            pitch: 0.0,
            chunk_count: 0,
            hud: Hud::default(),
            inventory: Inventory::default(),
        }
    }
}

impl Screen for GameScreen {
    fn title(&self) -> &'static str {
        "Game"
    }

    #[allow(deprecated)] // Panel::show
    fn ui(&mut self, ctx: &Context) -> Transition {
        let mut transition = Transition::None;

        // Handle toggle keys before building UI.
        self.hud.handle_input(ctx);

        // Sync inventory open state from hud.
        self.inventory.open = self.hud.inventory_open;

        // Escape toggles pause (but not if another screen consumed it).
        let esc_pressed = ctx.input(|i| i.key_pressed(egui::Key::Escape));
        if esc_pressed {
            self.paused = !self.paused;
        }

        // Top status bar.
        egui::Panel::top("game_hud").show(ctx, |ui| {
            ui.horizontal(|ui| {
                ui.colored_label(
                    egui::Color32::WHITE,
                    format!("FPS: {:.0}", self.fps),
                );
                ui.separator();
                ui.colored_label(
                    egui::Color32::from_gray(200),
                    "E: inventory   F3: debug   T: chat   Esc: menu",
                );
            });
        });

        // HUD elements: crosshair, debug panel, chat bar.
        self.hud.render(
            ctx,
            self.camera_pos,
            self.yaw,
            self.pitch,
            self.fps,
            self.chunk_count,
        );

        // Inventory overlay.
        self.inventory.render(ctx);

        // Pause menu overlay.
        if self.paused {
            egui::Window::new("Pause")
                .collapsible(false)
                .resizable(false)
                .anchor(egui::Align2::CENTER_CENTER, [0.0, 0.0])
                .show(ctx, |ui| {
                    ui.vertical_centered(|ui| {
                        ui.heading("Paused");
                        ui.add_space(10.0);
                    });

                    if ui.button("Resume").clicked() {
                        self.paused = false;
                    }

                    if ui.button("Options").clicked() {
                        transition =
                            Transition::Push(Box::new(OptionsScreen::default()));
                    }

                    ui.add_space(10.0);

                    if ui.button("Quit to Title").clicked() {
                        self.paused = false;
                        transition =
                            Transition::Push(Box::new(TitleScreen));
                    }
                });
        }

        transition
    }
}

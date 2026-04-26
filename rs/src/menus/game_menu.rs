//! In-game screen — HUD overlay and pause menu.
//!
//! The actual 3D world render pass is handled by `app.rs`. This screen
//! provides the HUD top bar, crosshair, debug panel, chat bar, inventory
//! window, and the pause menu (Escape). Unlike the menu screens, it doesn't
//! implement the [`Screen`] trait — its [`Self::tick`] takes a mutable
//! reference to the player + the block registry so the inventory can mutate
//! the player's stacks on click.
//!
//! "Save & Quit to Title" submits a [`WorldAction::LeaveToTitle`] rather than
//! pushing a `TitleScreen` directly — the app needs to save the world and
//! drop the live `Game` instance, which the screen layer can't do on its
//! own. The `OptionsScreen` is still pushed inline because it doesn't need
//! to interact with the world.

use std::sync::{Arc, Mutex};

use cgmath::{Matrix4, SquareMatrix};
use egui::Context;

use crate::ui::action::{WorldAction, WorldActionQueue};
use crate::ui::hud::{Hud, HudFrame};
use crate::ui::inventory::Inventory;
use crate::ui::screen::Transition;
use super::OptionsScreen;
use crate::blocks::{BlockRegistry, Id};
use crate::config::Config;
use crate::game::Hit;
use crate::worlds::Player;

/// The in-game screen shown during gameplay.
///
/// Composes [`Hud`] (crosshair, debug, chat) and [`Inventory`] (item grid).
/// Per-frame state (camera pose, FPS, selection) is set by the app before
/// `tick`; the chat history is queried via `chat_history` (a borrow into
/// `Game::chat_messages`).
pub struct GameScreen {
    pub paused: bool,
    pub fps: f32,
    pub camera_pos: [f64; 3],
    pub yaw: f64,
    pub pitch: f64,
    pub chunk_count: usize,
    /// Currently-selected block (raycast hit). Set by app before each frame.
    pub selected: Option<Hit>,
    /// View-projection matrix for the selection outline. Set by app.
    pub view_proj: Matrix4<f32>,
    /// Visible chat history (most-recent last). Set by app each frame.
    pub chat_history: Vec<String>,
    pub hud: Hud,
    pub inventory: Inventory,
    /// Shared with `App` and the menu screens. Forwarded into
    /// `OptionsScreen` so settings tweaks edit the live config.
    config: Arc<Mutex<Config>>,
    /// Mailbox for "leave to title" — the app reacts by saving + tearing
    /// down the world and pushing a fresh `TitleScreen`.
    actions: Arc<WorldActionQueue>,
}

impl GameScreen {
    #[must_use]
    pub fn new(config: Arc<Mutex<Config>>, actions: Arc<WorldActionQueue>) -> Self {
        Self {
            paused: false,
            fps: 0.0,
            camera_pos: [0.0; 3],
            yaw: 0.0,
            pitch: 0.0,
            chunk_count: 0,
            selected: None,
            view_proj: Matrix4::identity(),
            chat_history: Vec::new(),
            hud: Hud::default(),
            inventory: Inventory::default(),
            config,
            actions,
        }
    }

    /// Drive one frame of the in-game UI. Returns the same [`Transition`]
    /// the menu screens use — `Push` opens the options/pause screen,
    /// `Exit` quits the application.
    ///
    /// Takes `&mut Player` and `&BlockRegistry` so the inventory can move
    /// stacks around on click and so each slot can show the block's name.
    /// `air_id` lets the inventory paint empty slots without consulting the
    /// `BaseBlocks` table directly.
    #[allow(deprecated)] // Panel::show
    pub fn tick(
        &mut self,
        ctx: &Context,
        player: &mut Player,
        registry: &BlockRegistry,
        air_id: Id,
    ) -> Transition {
        let mut transition = Transition::None;

        // Handle toggle keys before building UI.
        self.hud.handle_input(ctx);

        // Sync inventory open state from hud.
        self.inventory.open = self.hud.inventory_open;

        // Escape toggles pause — but only when chat is closed (otherwise
        // the chat bar's own Escape handler closes the bar instead). When
        // the inventory is open, Escape closes the inventory instead of
        // opening the pause menu.
        if !self.hud.chat_open {
            let esc_pressed = ctx.input(|i| i.key_pressed(egui::Key::Escape));
            if esc_pressed {
                if self.hud.inventory_open {
                    self.hud.inventory_open = false;
                    self.inventory.open = false;
                } else {
                    self.paused = !self.paused;
                }
            }
        }

        // Top status bar.
        egui::Panel::top("game_hud").show(ctx, |ui| {
            ui.horizontal(|ui| {
                ui.colored_label(egui::Color32::WHITE, format!("FPS: {:.0}", self.fps));
                ui.separator();
                ui.colored_label(
                    egui::Color32::from_gray(200),
                    "E: inventory   F3: debug   T: chat   Esc: menu",
                );
            });
        });

        // HUD elements: crosshair, debug panel, selection outline, chat bar.
        let history: Vec<&str> = self.chat_history.iter().map(String::as_str).collect();
        let frame = HudFrame {
            camera_pos: self.camera_pos,
            yaw: self.yaw,
            pitch: self.pitch,
            fps: self.fps,
            chunk_count: self.chunk_count,
            selected: self.selected,
            view_proj: self.view_proj,
            chat_history: &history,
        };
        self.hud.render(ctx, &frame);

        // Inventory overlay (always renders the hotbar; renders the full
        // grid only when `inventory_open`).
        self.inventory.render(ctx, player, registry, air_id);

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
                        transition = Transition::Push(Box::new(OptionsScreen::new(
                            Arc::clone(&self.config),
                        )));
                    }

                    ui.add_space(10.0);

                    if ui.button("Save & Quit to Title").clicked() {
                        self.paused = false;
                        // The app drains `WorldAction::LeaveToTitle`, saves
                        // the world, drops the `Game`, and pushes a fresh
                        // `TitleScreen`. Nothing to push on the stack here.
                        self.actions.submit(WorldAction::LeaveToTitle);
                    }
                });
        }

        transition
    }
}

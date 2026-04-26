//! In-game screen — HUD overlay, inventory, and pause menu.
//!
//! The actual 3D world render pass is handled by `app.rs`. This screen
//! provides the HUD top bar, crosshair, debug panel, chat bar, inventory
//! window, and the pause menu (Escape). Unlike the menu screens, it doesn't
//! implement the [`Screen`] trait — its [`Self::tick`] takes a mutable
//! reference to the player + the block registry so the inventory can mutate
//! the player's stacks on click.
//!
//! The pause overlay mirrors `old/src/menus/game_menu.cpp`: a centred column
//! holding the caption, then a Back-to-title / Continue pair row. It sits
//! inside an `egui::Window` so the player can still see the live world
//! behind it. "Save & Quit to Title" submits a [`WorldAction::LeaveToTitle`]
//! rather than pushing a `TitleScreen` directly — the app needs to save the
//! world and drop the live `Game`. The `OptionsScreen` is still pushed
//! inline because it doesn't need to interact with the world.

use std::sync::{Arc, Mutex};

use egui::Context;

use super::{
    MENU_ROW_SPACING, OptionsScreen, caption_row, full_row_button, menu_overlay, pair_row, t,
};
use crate::blocks::{BlockRegistry, Id};
use crate::config::Config;
use crate::globalization::I18n;
use crate::ui::action::{WorldAction, WorldActionQueue};
use crate::ui::hud::{Hud, HudFrame};
use crate::ui::inventory::Inventory;
use crate::ui::screen::Transition;
use crate::worlds::Player;

/// The in-game screen shown during gameplay.
///
/// Composes [`Hud`] (crosshair, debug, chat) and [`Inventory`] (item grid).
/// Per-frame state (camera pose, FPS) is set by the app before `tick`; the
/// chat history is queried via `chat_history` (a borrow into
/// `Game::chat_messages`).
pub struct GameScreen {
    pub paused: bool,
    pub fps: f32,
    pub camera_pos: [f64; 3],
    pub yaw: f64,
    pub pitch: f64,
    pub chunk_count: usize,
    /// Visible chat history (most-recent last). Set by app each frame.
    pub chat_history: Vec<String>,
    pub hud: Hud,
    pub inventory: Inventory,
    /// Shared with `App` and the menu screens. Forwarded into
    /// `OptionsScreen` so settings tweaks edit the live config.
    config: Arc<Mutex<Config>>,
    /// Active language table — passed through so the pause menu strings
    /// stay in sync with the language picker.
    i18n: Arc<Mutex<I18n>>,
    /// Mailbox for "leave to title" — the app reacts by saving + tearing
    /// down the world and pushing a fresh `TitleScreen`.
    actions: Arc<WorldActionQueue>,
}

impl GameScreen {
    #[must_use]
    pub fn new(
        config: Arc<Mutex<Config>>,
        i18n: Arc<Mutex<I18n>>,
        actions: Arc<WorldActionQueue>,
    ) -> Self {
        Self {
            paused: false,
            fps: 0.0,
            camera_pos: [0.0; 3],
            yaw: 0.0,
            pitch: 0.0,
            chunk_count: 0,
            chat_history: Vec::new(),
            hud: Hud::default(),
            inventory: Inventory::default(),
            config,
            i18n,
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
    /// `BaseBlocks` table directly. `block_icons` indexes the egui texture id
    /// for each layer of the block-diffuse atlas (built by `App::resumed`),
    /// passed through so each inventory slot can paint its block art.
    #[allow(deprecated)] // Panel::show
    pub fn tick(
        &mut self,
        ctx: &Context,
        player: &mut Player,
        registry: &BlockRegistry,
        air_id: Id,
        block_icons: &[egui::TextureId],
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
                    "E: inventory   F3: debug   T: chat   F11: fullscreen   Esc: menu",
                );
            });
        });

        // HUD elements: crosshair, debug panel, chat bar. The selection
        // wireframe runs as a real 3-D pass via `SelectionPipeline` — see
        // `Game::record_world_pass`.
        let history: Vec<&str> = self.chat_history.iter().map(String::as_str).collect();
        let frame = HudFrame {
            camera_pos: self.camera_pos,
            yaw: self.yaw,
            pitch: self.pitch,
            fps: self.fps,
            chunk_count: self.chunk_count,
            chat_history: &history,
        };
        self.hud.render(ctx, &frame);

        // Inventory overlay (always renders the hotbar; renders the full
        // grid only when `inventory_open`).
        self.inventory.render(ctx, player, registry, air_id, block_icons);

        // Pause menu overlay — mirror of `old/src/menus/game_menu.cpp`.
        // Uses the same `caption_row` + `pair_row` chrome as every other
        // menu so button widths, row heights, and column gutters match
        // exactly. The pause variant uses `menu_overlay` (no opaque
        // background) so the live HUD / crosshair / inventory remain
        // visible behind it — diverges from the C++ build, which
        // takes a full screenshot of the gameplay frame and freezes
        // it as a backdrop.
        if self.paused {
            let caption = t(&self.i18n, "NEWorld.pause.caption");
            let back_lbl = t(&self.i18n, "NEWorld.pause.back");
            let continue_lbl = t(&self.i18n, "NEWorld.pause.continue");
            let options_lbl = t(&self.i18n, "NEWorld.main.options");

            let mut want_resume = false;
            let mut want_options = false;
            let mut want_leave = false;

            menu_overlay(ctx, "pause", |ui| {
                caption_row(ui, &caption);
                ui.add_space(MENU_ROW_SPACING);

                pair_row(ui, |cols| {
                    if cols[0].button(&back_lbl).clicked() {
                        want_leave = true;
                    }
                    if cols[1].button(&continue_lbl).clicked() {
                        want_resume = true;
                    }
                });
                ui.add_space(MENU_ROW_SPACING);

                // Options is not in the C++ pause-menu DSL but is useful
                // for tweaking sensitivity / FOV / language mid-game; the
                // C++ build exposes it via the F1 key. Keep it as a
                // full-width row below the matched pair.
                if full_row_button(ui, &options_lbl) {
                    want_options = true;
                }
            });

            if want_resume {
                self.paused = false;
            } else if want_options {
                transition = Transition::Push(Box::new(OptionsScreen::new(
                    Arc::clone(&self.config),
                    Arc::clone(&self.i18n),
                )));
            } else if want_leave {
                self.paused = false;
                self.actions.submit(WorldAction::LeaveToTitle);
            }
        }

        transition
    }
}

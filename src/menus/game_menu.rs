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
//! holding the caption, then Continue and a Back-to-title / Options row.
//! It uses [`crate::ui::show`] — the same path every out-of-game menu
//! takes — so the layout chrome (scrim, padding, max-width, theme-aware
//! widget colours) matches the rest of the menus exactly. Like the main
//! menu, the column is centred vertically (`MainAxisAlignment::Center`).
//! The live world keeps rendering behind because the world pass runs in
//! `app.rs` regardless of menu state. HUD + inventory are suppressed
//! while paused so they don't fight the pause widgets for screen real
//! estate.
//!
//! "Save & Quit to Title" submits a [`WorldAction::LeaveToTitle`] rather
//! than pushing a `TitleScreen` directly — the app needs to save the
//! world and drop the live `Game`. The `OptionsScreen` is still pushed
//! inline because it doesn't need to interact with the world.

use std::sync::{Arc, Mutex};

use egui::Context;

use super::action::{WorldAction, WorldActionQueue};
use super::screen::Transition;
use super::{
    MENU_COL_SPACING, MENU_MAX_WIDTH, MENU_PADDING, MENU_ROW_HEIGHT, MENU_ROW_SPACING,
    OptionsScreen, t,
};
use crate::blocks::{BlockRegistry, Id};
use crate::config::Config;
use crate::game::hud::{Hud, HudFrame};
use crate::game::inventory::Inventory;
use crate::globalization::I18n;
use crate::ui;
use crate::ui::widgets::{
    Aligned, Alignment, Button, CrossAxisSize, Flex, FlexItem, Label, MainAxisAlignment,
    MainAxisSize, Padding, Sizer, Spacer,
};
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
    pub ups: f32,
    pub camera_pos: [f64; 3],
    pub yaw: f64,
    pub pitch: f64,
    /// Total chunks loaded in `World` (including empty sky chunks).
    pub chunk_count: usize,
    /// Chunk meshes that survived camera frustum + distance culling on
    /// the last drawn frame. ≤ `chunk_count` and ≤ `meshed_chunks`.
    pub rendered_chunks: usize,
    /// Wrapping count of chunks unloaded since world load — running tally,
    /// not a current population.
    pub unloaded_chunks: u32,
    /// Chunks with an uploaded mesh. Mirrors `Game::chunk_meshes.len()`.
    pub meshed_chunks: usize,
    /// Player game-mode-driven flags surfaced in the F3 panel.
    pub creative: bool,
    pub cross_wall: bool,
    pub grounded: bool,
    pub near_wall: bool,
    pub in_water: bool,
    /// World tick counter (30 Hz). C++ formats as h:m:s plus the raw value.
    pub game_time: u32,
    /// Wrapping count of block updates resolved since world load.
    pub updated_blocks: u32,
    /// Pending entries in `World::block_update_queue`.
    pub pending_block_updates: usize,
    pub advanced_render: bool,
    pub show_shadow_map: bool,
    /// Renderer backend label (e.g. "Vulkan", "DX12"), pulled once at
    /// startup. Mirrors the C++ `[GL {Major}.{Minor}]` annotation.
    pub backend: &'static str,
    /// Visible chat history (most-recent last). Set by app each frame.
    pub chat_history: Vec<String>,
    /// Pre-formatted "(x, y, z) id N (name)" line describing the
    /// currently-selected block, or `None` if the raycast missed. Set
    /// by the app each frame; rendered by the F3 debug panel.
    pub selected_block: Option<String>,
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
            ups: 0.0,
            camera_pos: [0.0; 3],
            yaw: 0.0,
            pitch: 0.0,
            chunk_count: 0,
            rendered_chunks: 0,
            unloaded_chunks: 0,
            meshed_chunks: 0,
            creative: false,
            cross_wall: false,
            grounded: false,
            near_wall: false,
            in_water: false,
            game_time: 0,
            updated_blocks: 0,
            pending_block_updates: 0,
            advanced_render: false,
            show_shadow_map: false,
            backend: "wgpu",
            chat_history: Vec::new(),
            selected_block: None,
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

        if self.paused {
            // Pause overlay — same render path as every other menu
            // (`ui::show` → transparent CentralPanel + theme scrim), so
            // chrome and theming match the title / options / language
            // screens exactly. Buttons are vertically centred via
            // `MainAxisAlignment::Center`, mirroring the main menu.
            // HUD and inventory are intentionally suppressed below so
            // they don't compete with the pause widgets; the live
            // world is still drawn behind us by `Game::record_world_pass`.
            let caption = t(&self.i18n, "NEWorld.pause.caption");
            let back_lbl = t(&self.i18n, "NEWorld.pause.back");
            let continue_lbl = t(&self.i18n, "NEWorld.pause.continue");
            let options_lbl = t(&self.i18n, "NEWorld.main.options");

            let mut want_resume = false;
            let mut want_options = false;
            let mut want_leave = false;

            let body = Flex::column(vec![
                FlexItem::new(Sizer::height(
                    MENU_ROW_HEIGHT,
                    Aligned::center(Label::new(&caption)),
                )),
                FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
                FlexItem::new(Sizer::height(
                    MENU_ROW_HEIGHT,
                    Button::new(&continue_lbl).clicked(&mut want_resume),
                )),
                FlexItem::new(Spacer::height(MENU_ROW_SPACING)),
                FlexItem::new(Sizer::height(
                    MENU_ROW_HEIGHT,
                    Flex::row(vec![
                        FlexItem::flex(1.0, Button::new(&back_lbl).clicked(&mut want_leave)),
                        FlexItem::new(Spacer::width(MENU_COL_SPACING)),
                        FlexItem::flex(1.0, Button::new(&options_lbl).clicked(&mut want_options)),
                    ])
                    .main_size(MainAxisSize::Max)
                    .cross_size(CrossAxisSize::Max),
                )),
            ])
            .main_size(MainAxisSize::Max)
            .main_align(MainAxisAlignment::Center)
            .cross_size(CrossAxisSize::Max);

            let root = Aligned::new(
                Alignment::TopCenter,
                Padding::all(MENU_PADDING, Sizer::width(MENU_MAX_WIDTH, body)),
            );

            ui::show(ctx, root);

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
        } else {
            // Live HUD path — crosshair, debug panel, chat bar, hotbar,
            // and (when open) the full inventory. The selection
            // wireframe runs as a real 3-D pass via `SelectionPipeline`
            // — see `Game::record_world_pass`.
            let history: Vec<&str> = self.chat_history.iter().map(String::as_str).collect();
            let frame = HudFrame {
                camera_pos: self.camera_pos,
                yaw: self.yaw,
                pitch: self.pitch,
                fps: self.fps,
                ups: self.ups,
                chunk_count: self.chunk_count,
                rendered_chunks: self.rendered_chunks,
                unloaded_chunks: self.unloaded_chunks,
                meshed_chunks: self.meshed_chunks,
                creative: self.creative,
                cross_wall: self.cross_wall,
                grounded: self.grounded,
                near_wall: self.near_wall,
                in_water: self.in_water,
                game_time: self.game_time,
                updated_blocks: self.updated_blocks,
                pending_block_updates: self.pending_block_updates,
                advanced_render: self.advanced_render,
                show_shadow_map: self.show_shadow_map,
                backend: self.backend,
                chat_history: &history,
                selected_block: self.selected_block.as_deref(),
            };
            self.hud.render(ctx, &frame);
            self.inventory
                .render(ctx, player, registry, air_id, block_icons);
        }

        transition
    }
}

//! Application bring-up — winit `ApplicationHandler` plus the per-frame
//! render entry point.
//!
//! Owns the window, the wgpu context (`Gfx`), the shared atlases and frame
//! uniform buffer, the HUD text renderer, the egui renderer, the screen
//! stack, and the [`Game`] instance that holds the static world + camera +
//! chunk meshes.
//!
//! Per `docs/rust_migration.md` §5, tasks [E] (UI / menus / HUD / inventory)
//! are implemented on top of egui 0.34. [F] (raycast, breaking, async
//! pipeline, save/load) is still skipped.

use std::path::PathBuf;
use std::sync::Arc;
use std::time::Instant;

use winit::application::ApplicationHandler;
use winit::dpi::LogicalSize;
use winit::error::EventLoopError;
use winit::event::{
    DeviceEvent, DeviceId, ElementState, MouseButton as WinitMouseButton, WindowEvent,
};
use winit::event_loop::{ActiveEventLoop, ControlFlow, EventLoop};
use winit::keyboard::{KeyCode, PhysicalKey};
use winit::window::{CursorGrabMode, Window, WindowAttributes, WindowId};

use crate::game::{build_block_registry, Game};
use crate::gfx::{
    Atlases, EguiRenderer, FrameUniforms, Gfx, TextRenderer, UniformBuffer,
};
use crate::input::{InputState, Key, MouseButton};
use crate::math::Vec2f;
use crate::ui::{GameScreen, Screen, ScreenStack, Transition};

/// Per-window runtime state. Created on the first `resumed` event.
struct AppState {
    window: Arc<Window>,
    gfx: Gfx,
    /// Held to keep the wgpu textures alive — `Game`'s bind groups reference
    /// these texture views.
    #[allow(dead_code)]
    atlases: Atlases,
    frame_uniforms: UniformBuffer<FrameUniforms>,
    /// Held for the GPU resources; egui superseded the debug-text HUD, but
    /// `TextRenderer` may be re-used for chat rendering.
    #[allow(dead_code)]
    text: TextRenderer,
    game: Game,
    input: InputState,
    last_tick: Instant,
    start_time: Instant,
    cursor_grabbed: bool,
    egui_renderer: EguiRenderer,
    /// The in-game screen (HUD, crosshair, inventory, pause menu).
    /// Rendered when the screen stack is empty.
    game_screen: GameScreen,
    /// Menu screens overlaid on top of the game. When non-empty, the top
    /// screen receives input and the cursor is released.
    screen_stack: ScreenStack,
    /// Set by `Transition::Exit` from a screen; causes the event loop to exit.
    exit_requested: bool,
}

/// Application root. Implements [`winit::application::ApplicationHandler`].
#[derive(Default)]
pub struct App {
    state: Option<AppState>,
}

impl App {
    /// Build an event loop and run the application until exit.
    pub fn run() -> Result<(), EventLoopError> {
        let event_loop = EventLoop::new()?;
        event_loop.set_control_flow(ControlFlow::Poll);
        let mut app = Self::default();
        event_loop.run_app(&mut app)
    }

    /// Resolve the assets directory at compile time so the binary can locate
    /// PNGs no matter where it's launched from.
    fn assets_root() -> PathBuf {
        if let Some(dir) = option_env!("CARGO_MANIFEST_DIR") {
            return PathBuf::from(dir).join("assets");
        }
        PathBuf::from("rs").join("assets")
    }

    /// Tick + render one frame. Returns `true` if the app should exit.
    fn frame(state: &mut AppState) -> bool {
        // ---------- timing ----------
        let now = Instant::now();
        let dt = (now - state.last_tick).as_secs_f32().min(0.1);
        state.last_tick = now;
        let elapsed = (now - state.start_time).as_secs_f32();

        // ---------- simulation ----------
        state.game.tick(dt, &state.input);

        // ---------- frame uniforms ----------
        let surface_size = state.gfx.surface_size();
        state
            .game
            .write_frame_uniforms(state.gfx.queue(), &state.frame_uniforms, surface_size, elapsed);

        // ---------- consume per-frame input transients ----------
        state.input.begin_frame();

        // ---------- update game screen with latest frame data ----------
        let fps = if dt > 0.0 { 1.0 / dt } else { 0.0 };
        state.game_screen.fps = fps;
        state.game_screen.camera_pos = [
            state.game.camera.position.x,
            state.game.camera.position.y,
            state.game.camera.position.z,
        ];
        state.game_screen.yaw = state.game.camera.yaw;
        state.game_screen.pitch = state.game.camera.pitch;
        state.game_screen.chunk_count = state.game.chunk_meshes.len();

        // Game screen paused state freezes cursor grab.
        let game_paused = state.game_screen.paused;

        // ---------- begin egui frame ----------
        state.egui_renderer.begin_frame(&state.window);

        // ---------- tick UI (game screen or menu stack) ----------
        let ctx = state.egui_renderer.context();
        if state.screen_stack.is_empty() {
            // In-game: tick the game screen directly.
            match state.game_screen.ui(ctx) {
                Transition::Push(s) => state.screen_stack.push(s),
                Transition::Exit => {
                    state.exit_requested = true;
                    return true;
                }
                _ => {}
            }
        } else {
            // Menus overlaying the game: tick the screen stack.
            let action = state.screen_stack.tick(ctx);
            if action {
                // A screen requested exit.
                state.exit_requested = true;
                return true;
            }
        }

        // ---------- end egui frame (tessellates) ----------
        state.egui_renderer.end_frame(&state.window);

        // ---------- upload egui textures (glyph atlas etc.) ----------
        state
            .egui_renderer
            .update_textures(state.gfx.device(), state.gfx.queue());

        // ---------- cursor grab logic ----------
        // Grab when in-game and not paused; release when menus are open.
        if state.screen_stack.is_empty() && !game_paused {
            Self::grab_cursor(&state.window, &mut state.cursor_grabbed);
        } else {
            Self::release_cursor(&state.window, &mut state.cursor_grabbed);
        }

        // ---------- acquire surface ----------
        let frame = match state.gfx.acquire() {
            wgpu::CurrentSurfaceTexture::Success(tex)
            | wgpu::CurrentSurfaceTexture::Suboptimal(tex) => tex,
            wgpu::CurrentSurfaceTexture::Lost | wgpu::CurrentSurfaceTexture::Outdated => {
                let (w, h) = state.gfx.surface_size();
                tracing::warn!(w, h, "surface lost/outdated; reconfiguring");
                state.gfx.reconfigure();
                state.window.request_redraw();
                return false;
            }
            wgpu::CurrentSurfaceTexture::Timeout => {
                tracing::warn!("surface acquire timed out; skipping frame");
                state.window.request_redraw();
                return false;
            }
            wgpu::CurrentSurfaceTexture::Occluded => {
                return false;
            }
            wgpu::CurrentSurfaceTexture::Validation => {
                tracing::error!("surface acquire raised validation error");
                state.window.request_redraw();
                return false;
            }
        };

        let view = frame
            .texture
            .create_view(&wgpu::TextureViewDescriptor::default());

        let mut encoder = state
            .gfx
            .device()
            .create_command_encoder(&wgpu::CommandEncoderDescriptor {
                label: Some("neworld.frame_encoder"),
            });

        // ---------- upload egui buffers into encoder ----------
        let extra_cbs = state.egui_renderer.update_buffers(
            state.gfx.device(),
            state.gfx.queue(),
            &mut encoder,
        );

        // ---------- world render pass (clears to sky) ----------
        state
            .game
            .record_world_pass(state.gfx.device(), &mut encoder, &view);

        // ---------- egui render pass (on top of the world) ----------
        {
            let mut egui_pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("neworld.egui_pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &view,
                    depth_slice: None,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Load,
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: None,
                timestamp_writes: None,
                occlusion_query_set: None,
                multiview_mask: None,
            });
            state.egui_renderer.render(&mut egui_pass);
        }

        // ---------- submit & present ----------
        state
            .gfx
            .queue()
            .submit(std::iter::once(encoder.finish()).chain(extra_cbs));
        frame.present();
        state.window.request_redraw();

        false
    }

    /// Try to lock the cursor to the window center for FPS-style mouse-look.
    fn grab_cursor(window: &Window, state: &mut bool) {
        if *state {
            return;
        }
        let result = window
            .set_cursor_grab(CursorGrabMode::Locked)
            .or_else(|_| window.set_cursor_grab(CursorGrabMode::Confined));
        if let Err(err) = result {
            tracing::warn!(error = %err, "failed to grab cursor");
            return;
        }
        window.set_cursor_visible(false);
        *state = true;
    }

    fn release_cursor(window: &Window, state: &mut bool) {
        if !*state {
            return;
        }
        let _ = window.set_cursor_grab(CursorGrabMode::None);
        window.set_cursor_visible(true);
        *state = false;
    }
}

/// Translate a winit `KeyCode` into our crate-local [`Key`] enum, or `None`
/// if we don't track the key.
fn translate_key(code: KeyCode) -> Option<Key> {
    Some(match code {
        KeyCode::KeyA => Key::A,
        KeyCode::KeyD => Key::D,
        KeyCode::KeyE => Key::E,
        KeyCode::KeyF => Key::F,
        KeyCode::KeyG => Key::G,
        KeyCode::KeyH => Key::H,
        KeyCode::KeyL => Key::L,
        KeyCode::KeyM => Key::M,
        KeyCode::KeyR => Key::R,
        KeyCode::KeyS => Key::S,
        KeyCode::KeyW => Key::W,
        KeyCode::KeyX => Key::X,
        KeyCode::KeyZ => Key::Z,
        KeyCode::Slash => Key::Slash,
        KeyCode::Space => Key::Space,
        KeyCode::Tab => Key::Tab,
        KeyCode::Enter => Key::Enter,
        KeyCode::Backspace => Key::Backspace,
        KeyCode::Escape => Key::Escape,
        KeyCode::ShiftLeft => Key::LeftShift,
        KeyCode::ShiftRight => Key::RightShift,
        KeyCode::ControlLeft => Key::LeftControl,
        KeyCode::ControlRight => Key::RightControl,
        KeyCode::AltLeft => Key::LeftAlt,
        KeyCode::AltRight => Key::RightAlt,
        KeyCode::F1 => Key::F1,
        KeyCode::F2 => Key::F2,
        KeyCode::F3 => Key::F3,
        KeyCode::F4 => Key::F4,
        KeyCode::F5 => Key::F5,
        KeyCode::F6 => Key::F6,
        KeyCode::F7 => Key::F7,
        KeyCode::F8 => Key::F8,
        KeyCode::ArrowLeft => Key::ArrowLeft,
        KeyCode::ArrowRight => Key::ArrowRight,
        KeyCode::ArrowUp => Key::ArrowUp,
        KeyCode::ArrowDown => Key::ArrowDown,
        _ => return None,
    })
}

fn translate_mouse_button(b: WinitMouseButton) -> Option<MouseButton> {
    Some(match b {
        WinitMouseButton::Left => MouseButton::Left,
        WinitMouseButton::Right => MouseButton::Right,
        WinitMouseButton::Middle => MouseButton::Middle,
        WinitMouseButton::Back => MouseButton::X1,
        WinitMouseButton::Forward => MouseButton::X2,
        WinitMouseButton::Other(_) => return None,
    })
}

impl ApplicationHandler for App {
    fn resumed(&mut self, event_loop: &ActiveEventLoop) {
        if self.state.is_some() {
            return;
        }

        let attributes = WindowAttributes::default()
            .with_title("NEWorld")
            .with_inner_size(LogicalSize::new(1280, 720))
            .with_min_inner_size(LogicalSize::new(256, 144));

        let window = match event_loop.create_window(attributes) {
            Ok(w) => Arc::new(w),
            Err(err) => {
                tracing::error!(error = %err, "failed to create window");
                event_loop.exit();
                return;
            }
        };

        let gfx = Gfx::new(window.clone());

        let assets = Self::assets_root();
        let atlases = match Atlases::load(gfx.device(), gfx.queue(), &assets) {
            Ok(a) => {
                tracing::info!(
                    diffuse_layers = a.block_diffuse.layers,
                    normal_layers = a.block_normal.layers,
                    "atlases loaded"
                );
                a
            }
            Err(err) => {
                tracing::error!(error = %err, "fatal: atlas load failed");
                event_loop.exit();
                return;
            }
        };

        let frame_uniforms =
            UniformBuffer::<FrameUniforms>::new(gfx.device(), "neworld.frame_uniforms");

        let text = TextRenderer::new(gfx.device(), gfx.queue(), gfx.surface_format());

        let egui_renderer = EguiRenderer::new(
            gfx.device(),
            gfx.surface_format(),
            &window,
            window.scale_factor() as f32,
        );

        // Build registry + base blocks once for the world generator and the
        // mesher (which both consume the same registry).
        let (registry, base) = build_block_registry();
        let game = match Game::new(
            gfx.device(),
            gfx.queue(),
            gfx.surface_format(),
            gfx.surface_size(),
            &registry,
            base,
            &frame_uniforms,
            &atlases,
        ) {
            Ok(g) => g,
            Err(err) => {
                tracing::error!(error = %err, "fatal: game init failed");
                event_loop.exit();
                return;
            }
        };

        window.request_redraw();

        let now = Instant::now();
        self.state = Some(AppState {
            window,
            gfx,
            atlases,
            frame_uniforms,
            text,
            game,
            input: InputState::new(),
            last_tick: now,
            start_time: now,
            cursor_grabbed: false,
            egui_renderer,
            game_screen: GameScreen::default(),
            screen_stack: ScreenStack::new(),
            exit_requested: false,
        });
        tracing::info!("app ready");
    }

    fn window_event(
        &mut self,
        event_loop: &ActiveEventLoop,
        _window_id: WindowId,
        event: WindowEvent,
    ) {
        let Some(state) = self.state.as_mut() else {
            return;
        };

        // Check exit flag (set by screen stack or close request).
        if state.exit_requested {
            event_loop.exit();
            return;
        }

        // Pass all events to egui for modifier / focus tracking.
        // We check egui_wants_focus before processing game keyboard input
        // so typing in an egui text field doesn't also move the player.
        let egui_resp = state.egui_renderer.handle_event(&state.window, &event);
        let egui_wants_focus = egui_resp.consumed;

        match event {
            WindowEvent::CloseRequested => {
                tracing::info!("close requested; exiting");
                event_loop.exit();
            }
            WindowEvent::Resized(size) => {
                state.gfx.resize(size.width, size.height);
                state
                    .game
                    .resize(state.gfx.device(), size.width, size.height);
                state
                    .egui_renderer
                    .set_scale_factor(state.window.scale_factor() as f32);
                state.window.request_redraw();
            }
            WindowEvent::RedrawRequested => {
                let should_exit = Self::frame(state);
                if should_exit {
                    event_loop.exit();
                }
            }
            WindowEvent::KeyboardInput { event, .. } => {
                if egui_wants_focus {
                    return;
                }
                let PhysicalKey::Code(code) = event.physical_key else {
                    return;
                };
                let Some(key) = translate_key(code) else {
                    return;
                };
                match event.state {
                    ElementState::Pressed => {
                        // Release cursor on Escape (handled by game screen now,
                        // but keep as safety for when menus are not open).
                        if key == Key::Escape && state.cursor_grabbed {
                            Self::release_cursor(&state.window, &mut state.cursor_grabbed);
                        }
                        if !event.repeat && state.input.keys_down.insert(key) {
                            state.input.keys_pressed.insert(key);
                        }
                    }
                    ElementState::Released => {
                        if state.input.keys_down.remove(key) {
                            state.input.keys_released.insert(key);
                        }
                    }
                }
            }
            WindowEvent::MouseInput {
                state: btn_state,
                button,
                ..
            } => {
                if egui_wants_focus {
                    return;
                }
                let Some(button) = translate_mouse_button(button) else {
                    return;
                };
                match btn_state {
                    ElementState::Pressed => {
                        if !state.cursor_grabbed && state.screen_stack.is_empty() {
                            Self::grab_cursor(&state.window, &mut state.cursor_grabbed);
                        }
                        if !state.input.mouse_buttons.contains(button) {
                            state.input.mouse_buttons.insert(button);
                            state.input.mouse_buttons_pressed.insert(button);
                        }
                    }
                    ElementState::Released => {
                        if state.input.mouse_buttons.contains(button) {
                            state.input.mouse_buttons.remove(button);
                            state.input.mouse_buttons_released.insert(button);
                        }
                    }
                }
            }
            WindowEvent::CursorMoved { position, .. } => {
                state.input.mouse_pos = Vec2f::new(position.x as f32, position.y as f32);
            }
            WindowEvent::MouseWheel { delta, .. } => {
                if egui_wants_focus {
                    return;
                }
                let dy = match delta {
                    winit::event::MouseScrollDelta::LineDelta(_, y) => y,
                    winit::event::MouseScrollDelta::PixelDelta(p) => p.y as f32 / 32.0,
                };
                state.input.mouse_wheel_delta += dy;
            }
            WindowEvent::Focused(false) => {
                Self::release_cursor(&state.window, &mut state.cursor_grabbed);
            }
            _ => {}
        }
    }

    fn device_event(
        &mut self,
        _event_loop: &ActiveEventLoop,
        _device_id: DeviceId,
        event: DeviceEvent,
    ) {
        let Some(state) = self.state.as_mut() else {
            return;
        };
        if let DeviceEvent::MouseMotion { delta: (dx, dy) } = event
            && state.cursor_grabbed
        {
            state.input.mouse_motion += Vec2f::new(dx as f32, dy as f32);
        }
    }
}

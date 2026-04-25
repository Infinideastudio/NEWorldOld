//! Application bring-up — winit `ApplicationHandler` plus the per-frame
//! render entry point.
//!
//! Owns the window, the wgpu context (`Gfx`), the shared atlases and frame
//! uniform buffer, the HUD text renderer, and the [`Game`] instance that
//! holds the static world + camera + chunk meshes.
//!
//! Per `docs/rust_migration.md` §5, the [E] (UI / menus) and [F] (raycast,
//! breaking, async pipeline, save/load) sub-tasks are skipped — `App` is the
//! minimum viable harness that loads a small, fully-generated world and
//! renders it with a free-fly camera.

use std::path::PathBuf;
use std::sync::Arc;
use std::time::Instant;

use winit::application::ApplicationHandler;
use winit::dpi::LogicalSize;
use winit::error::EventLoopError;
use winit::event::{DeviceEvent, DeviceId, ElementState, MouseButton as WinitMouseButton, WindowEvent};
use winit::event_loop::{ActiveEventLoop, ControlFlow, EventLoop};
use winit::keyboard::{KeyCode, PhysicalKey};
use winit::window::{CursorGrabMode, Window, WindowAttributes, WindowId};

use crate::game::{Game, build_block_registry};
use crate::gfx::{Atlases, FrameUniforms, Gfx, TextLine, TextRenderer, UniformBuffer};
use crate::input::{InputState, Key, MouseButton};
use crate::math::Vec2f;

/// HUD text color (white).
fn hud_color() -> glyphon::Color {
    glyphon::Color::rgb(0xFF, 0xFF, 0xFF)
}

/// Per-window runtime state. Created on the first `resumed` event.
struct AppState {
    window: Arc<Window>,
    gfx: Gfx,
    /// Held to keep the wgpu textures alive — `Game`'s bind groups reference
    /// these texture views.
    #[allow(dead_code)]
    atlases: Atlases,
    frame_uniforms: UniformBuffer<FrameUniforms>,
    text: TextRenderer,
    game: Game,
    input: InputState,
    last_tick: Instant,
    start_time: Instant,
    cursor_grabbed: bool,
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

    /// Tick + render one frame.
    fn frame(state: &mut AppState) {
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

        // ---------- consume per-frame input transients now that this frame
        //            has read them; new winit events for the next frame land
        //            after this. ----------
        state.input.begin_frame();

        // ---------- acquire surface ----------
        let frame = match state.gfx.acquire() {
            wgpu::CurrentSurfaceTexture::Success(tex)
            | wgpu::CurrentSurfaceTexture::Suboptimal(tex) => tex,
            wgpu::CurrentSurfaceTexture::Lost | wgpu::CurrentSurfaceTexture::Outdated => {
                let (w, h) = state.gfx.surface_size();
                tracing::warn!(w, h, "surface lost/outdated; reconfiguring");
                state.gfx.reconfigure();
                state.window.request_redraw();
                return;
            }
            wgpu::CurrentSurfaceTexture::Timeout => {
                tracing::warn!("surface acquire timed out; skipping frame");
                state.window.request_redraw();
                return;
            }
            wgpu::CurrentSurfaceTexture::Occluded => {
                return;
            }
            wgpu::CurrentSurfaceTexture::Validation => {
                tracing::error!("surface acquire raised validation error");
                state.window.request_redraw();
                return;
            }
        };

        let view = frame
            .texture
            .create_view(&wgpu::TextureViewDescriptor::default());

        let mut encoder =
            state
                .gfx
                .device()
                .create_command_encoder(&wgpu::CommandEncoderDescriptor {
                    label: Some("neworld.frame_encoder"),
                });

        // ---------- world render pass (clears to sky) ----------
        state.game.record_world_pass(state.gfx.device(), &mut encoder, &view);

        // ---------- HUD text overlay (load-op, paints over the world) ----------
        let (w, h) = state.gfx.surface_size();
        let camera = &state.game.camera;
        let hud = format!(
            "NEWorld (Rust port)   {w}x{h}   pos=({:.1}, {:.1}, {:.1})   yaw={:.2} pitch={:.2}   chunks={}   {:.1} fps",
            camera.position.x,
            camera.position.y,
            camera.position.z,
            camera.yaw,
            camera.pitch,
            state.game.chunk_meshes.len(),
            if dt > 0.0 { 1.0 / dt } else { 0.0 },
        );
        let help = if state.cursor_grabbed {
            "WSAD = move   Space/Shift = up/down   Ctrl = sprint   Esc = release mouse"
        } else {
            "click window to capture mouse"
        };
        let lines = [
            TextLine {
                text: &hud,
                x: 12.0,
                y: 10.0,
                scale: 1.0,
                color: hud_color(),
            },
            TextLine {
                text: help,
                x: 12.0,
                y: 30.0,
                scale: 1.0,
                color: hud_color(),
            },
        ];
        state
            .text
            .prepare(state.gfx.device(), state.gfx.queue(), (w, h), &lines);
        {
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("neworld.text_pass"),
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
            state.text.render(&mut pass);
        }

        state.gfx.queue().submit(std::iter::once(encoder.finish()));
        frame.present();
        state.window.request_redraw();
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
            .with_inner_size(LogicalSize::new(1280, 720));

        let window = match event_loop.create_window(attributes) {
            Ok(w) => Arc::new(w),
            Err(err) => {
                tracing::error!(error = %err, "failed to create window");
                event_loop.exit();
                return;
            }
        };

        let gfx = Gfx::new(window.clone());

        // Atlases are needed before we can build the chunk pipeline (the
        // pipeline's bind group references their texture views). Loading
        // happens against `assets/` discovered via CARGO_MANIFEST_DIR.
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
                state.window.request_redraw();
            }
            WindowEvent::RedrawRequested => {
                Self::frame(state);
            }
            WindowEvent::KeyboardInput { event, .. } => {
                let PhysicalKey::Code(code) = event.physical_key else {
                    return;
                };
                let Some(key) = translate_key(code) else {
                    return;
                };
                match event.state {
                    ElementState::Pressed => {
                        // Release cursor on Escape.
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
                let Some(button) = translate_mouse_button(button) else {
                    return;
                };
                match btn_state {
                    ElementState::Pressed => {
                        // First click also grabs the cursor.
                        if !state.cursor_grabbed {
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
                let dy = match delta {
                    winit::event::MouseScrollDelta::LineDelta(_, y) => y,
                    winit::event::MouseScrollDelta::PixelDelta(p) => p.y as f32 / 32.0,
                };
                state.input.mouse_wheel_delta += dy;
            }
            WindowEvent::Focused(false) => {
                // Release the cursor on focus loss so the user can alt-tab.
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

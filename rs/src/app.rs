//! Application bring-up — winit `ApplicationHandler` plus the per-frame
//! render entry point.
//!
//! Per `docs/rust_migration.md` §5, this is the integration point where the
//! `[C]` graphics sub-tasks meet:
//!
//! * `[C1]` `Gfx` owns the wgpu instance/surface/device/queue;
//! * `[C2]` `BasicPipeline` proves the wgpu pipeline path with an inline
//!   colored triangle;
//! * `[C3]` `Atlases` loads the block + UI PNGs at startup;
//! * `[C4]` `UniformBuffer<FrameUniforms>` is allocated and updated each
//!   frame (no shader consumes it yet — that lands with `[D]`);
//! * `[C5]` `TextRenderer` overlays a status line so glyphon is exercised
//!   on every frame.
//!
//! When the migration reaches `[F]`, this struct expands into the full
//! `GameApp` orchestrator (fixed-step tick, world ownership, screen stack).
//! For now `[C]` ships the minimum that puts pixels and glyphs on screen.

use std::path::PathBuf;
use std::sync::Arc;
use std::time::Instant;

use winit::application::ApplicationHandler;
use winit::dpi::LogicalSize;
use winit::error::EventLoopError;
use winit::event::WindowEvent;
use winit::event_loop::{ActiveEventLoop, ControlFlow, EventLoop};
use winit::window::{Window, WindowAttributes, WindowId};

use crate::gfx::{
    Atlases, BasicPipeline, FrameUniforms, Gfx, TextLine, TextRenderer, UniformBuffer,
};

/// Window background color (sRGB, pre-encoded). A muted slate blue so the
/// triangle and white text both stand out.
const CLEAR_COLOR: wgpu::Color = wgpu::Color {
    r: 0.05,
    g: 0.07,
    b: 0.12,
    a: 1.0,
};

/// HUD text color (white).
fn hud_color() -> glyphon::Color {
    glyphon::Color::rgb(0xFF, 0xFF, 0xFF)
}

/// Per-window runtime state. Created on the first `resumed` event.
struct AppState {
    window: Arc<Window>,
    gfx: Gfx,
    basic_pipeline: BasicPipeline,
    /// `None` if the assets directory could not be found at startup; the
    /// demo continues without textures rather than aborting, so we can still
    /// see the [C2] triangle and [C5] text.
    atlases: Option<Atlases>,
    frame_uniforms: UniformBuffer<FrameUniforms>,
    text: TextRenderer,
    start_time: Instant,
}

/// Application root. Implements [`winit::application::ApplicationHandler`].
///
/// `state` is `None` until `resumed` fires (winit 0.30 only allows window
/// creation from inside the event loop, after the platform has reported it
/// is safe to do so).
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

    /// Render a single frame.
    ///
    /// Records two passes against the surface texture:
    ///
    /// 1. **opaque pass** — clears the surface to [`CLEAR_COLOR`] then issues
    ///    [`BasicPipeline::draw`] for the scaffold triangle.
    /// 2. **text pass** — `LoadOp::Load` (preserves the triangle), then
    ///    [`TextRenderer::render`] paints the HUD line on top.
    ///
    /// Frame uniforms are written between passes so the GPU side observes the
    /// current viewport size and elapsed time even though no shader currently
    /// samples them — the write path is exercised end-to-end so [C4] is real.
    fn render(state: &mut AppState) {
        let frame = match state.gfx.acquire() {
            wgpu::CurrentSurfaceTexture::Success(tex) | wgpu::CurrentSurfaceTexture::Suboptimal(tex) => tex,
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
                // Window is hidden / minimized — let winit redeliver redraw
                // requests once we're visible again.
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

        // Pass 1: clear + scaffold triangle.
        {
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("neworld.opaque_pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &view,
                    depth_slice: None,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Clear(CLEAR_COLOR),
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: None,
                timestamp_writes: None,
                occlusion_query_set: None,
                multiview_mask: None,
            });
            state.basic_pipeline.draw(&mut pass);
        }

        // Update [C4] frame uniforms — no shader consumes them yet, but the
        // queue write exercises the typed wrapper end-to-end. Constructing via
        // `Default` then assigning the data fields avoids touching the
        // private `_pad` slot.
        let (w, h) = state.gfx.surface_size();
        let elapsed = state.start_time.elapsed().as_secs_f32();
        let mut uniforms = FrameUniforms::default();
        uniforms.screen_size = [w as f32, h as f32];
        uniforms.time = elapsed;
        state.frame_uniforms.write(state.gfx.queue(), &uniforms);

        // Build the HUD line. Static text + dynamic resolution + atlas count.
        let layers = state
            .atlases
            .as_ref()
            .map_or(0, |a| a.block_diffuse.layers);
        let hud = format!(
            "NEWorld (Rust port) — [C] graphics scaffolding   {w}x{h}   atlases={layers} layers   t={elapsed:.1}s"
        );
        let lines = [TextLine {
            text: &hud,
            x: 16.0,
            y: 16.0,
            scale: 1.0,
            color: hud_color(),
        }];
        state
            .text
            .prepare(state.gfx.device(), state.gfx.queue(), (w, h), &lines);

        // Pass 2: text on top of the triangle (LoadOp::Load preserves it).
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
}

/// Resolve the assets directory at runtime. Falls back to
/// `$CARGO_MANIFEST_DIR/assets` when set (the dev workflow); otherwise
/// `./rs/assets` relative to the executable's working dir, matching the
/// shipped layout.
fn assets_root() -> PathBuf {
    if let Some(dir) = option_env!("CARGO_MANIFEST_DIR") {
        return PathBuf::from(dir).join("assets");
    }
    PathBuf::from("rs").join("assets")
}

impl ApplicationHandler for App {
    fn resumed(&mut self, event_loop: &ActiveEventLoop) {
        if self.state.is_some() {
            // Already initialised; some platforms emit a second `resumed`
            // on re-show (e.g. Android). Keep the existing window.
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

        let basic_pipeline = BasicPipeline::new(gfx.device(), gfx.surface_format());

        let assets = assets_root();
        let atlases = match Atlases::load(gfx.device(), gfx.queue(), &assets) {
            Ok(a) => {
                tracing::info!(
                    diffuse_layers = a.block_diffuse.layers,
                    normal_layers = a.block_normal.layers,
                    "atlases loaded"
                );
                Some(a)
            }
            Err(err) => {
                tracing::warn!(error = %err, "atlas load failed; running without textures");
                None
            }
        };

        let frame_uniforms =
            UniformBuffer::<FrameUniforms>::new(gfx.device(), "neworld.frame_uniforms");

        let text = TextRenderer::new(gfx.device(), gfx.queue(), gfx.surface_format());

        // Drive the first frame; subsequent frames are scheduled by the
        // render path itself via `Window::request_redraw`.
        window.request_redraw();

        self.state = Some(AppState {
            window,
            gfx,
            basic_pipeline,
            atlases,
            frame_uniforms,
            text,
            start_time: Instant::now(),
        });
        tracing::info!("app ready (window + wgpu + pipeline + text)");
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
                state.window.request_redraw();
            }
            WindowEvent::RedrawRequested => {
                Self::render(state);
            }
            _ => {}
        }
    }
}

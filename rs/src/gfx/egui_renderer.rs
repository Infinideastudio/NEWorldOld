//! `EguiRenderer` — egui integration for the wgpu + winit stack.
//!
//! Wraps an [`egui::Context`], [`egui_winit::State`], and
//! [`egui_wgpu::Renderer`] so that [`crate::app::App`] can build immediate-mode
//! UI panels (HUD, inventory, menus) and composite them on top of the 3-D world.

use egui_wgpu::ScreenDescriptor;

/// Wgpu `RenderPass` lifetime-forget helper.
///
/// `egui_wgpu::Renderer::render` requires `&mut RenderPass<'static>`, but
/// wgpu ties the pass lifetime to the parent encoder. This transmute is
/// sound because the render pass does not actually borrow anything with
/// its lifetime parameter — see wgpu#1671.
#[allow(unsafe_code)]
fn forget_render_pass_lifetime<'pass>(
    pass: &'pass mut wgpu::RenderPass<'_>,
) -> &'pass mut wgpu::RenderPass<'static> {
    unsafe { std::mem::transmute(pass) }
}

/// Complete egui-on-wgpu renderer state.
///
/// Owns the egui context, the winit event → egui input translation layer,
/// the wgpu renderer, and the per-frame tessellation cache produced by
/// [`Self::end_frame`].
pub struct EguiRenderer {
    context: egui::Context,
    state: egui_winit::State,
    renderer: egui_wgpu::Renderer,
    paint_jobs: Vec<egui::ClippedPrimitive>,
    screen_descriptor: ScreenDescriptor,
    /// Accumulated texture delta from the most recent [`Self::end_frame`].
    /// Must be consumed via [`Self::update_textures`] before the next frame.
    textures_delta: egui::TexturesDelta,
}

impl EguiRenderer {
    /// Create a new egui renderer.
    ///
    /// `scale_factor` should be the window's current DPI scale
    /// (`window.scale_factor() as f32`).
    #[must_use]
    pub fn new(
        device: &wgpu::Device,
        surface_format: wgpu::TextureFormat,
        window: &winit::window::Window,
        scale_factor: f32,
    ) -> Self {
        let context = egui::Context::default();

        let state = egui_winit::State::new(
            context.clone(),
            egui::viewport::ViewportId::ROOT,
            window,
            Some(scale_factor),
            None,
            None,
        );

        let options = egui_wgpu::RendererOptions {
            msaa_samples: 0,
            depth_stencil_format: None,
            dithering: true,
            predictable_texture_filtering: false,
        };
        let renderer = egui_wgpu::Renderer::new(device, surface_format, options);

        let size = window.inner_size();
        let screen_descriptor = ScreenDescriptor {
            size_in_pixels: [size.width, size.height],
            pixels_per_point: scale_factor,
        };

        Self {
            context,
            state,
            renderer,
            paint_jobs: Vec::new(),
            screen_descriptor,
            textures_delta: egui::TexturesDelta::default(),
        }
    }

    /// The egui context that callers use to build UI between
    /// [`Self::begin_frame`] and [`Self::end_frame`].
    #[must_use]
    pub fn context(&self) -> &egui::Context {
        &self.context
    }

    /// Feed a winit window event to egui.
    ///
    /// Returns the event response. If `consumed` is true, the game layer
    /// should consider skipping its own handling (e.g. keyboard input when
    /// a text field has focus).
    ///
    /// `RedrawRequested` returns `EventResponse { consumed: false, repaint:
    /// false }` — the frame loop drives repaints itself.
    pub fn handle_event(
        &mut self,
        window: &winit::window::Window,
        event: &winit::event::WindowEvent,
    ) -> egui_winit::EventResponse {
        if matches!(event, winit::event::WindowEvent::RedrawRequested) {
            return egui_winit::EventResponse {
                consumed: false,
                repaint: false,
            };
        }
        self.state.on_window_event(window, event)
    }

    /// Begin a new egui frame.
    ///
    /// Call once per rendered frame, before any widget-building code.
    /// After this, callers build UI via [`Self::context`] methods (e.g.
    /// `egui::CentralPanel::default().show(self.context(), |ui| { ... })`).
    pub fn begin_frame(&mut self, window: &winit::window::Window) {
        let raw_input = self.state.take_egui_input(window);
        self.context.begin_pass(raw_input);
    }

    /// End the egui frame — runs layout, handles platform output
    /// (clipboard, cursor), tessellates draw data, and stores the paint
    /// jobs for later upload.
    pub fn end_frame(&mut self, window: &winit::window::Window) {
        let full_output = self.context.end_pass();

        self.state
            .handle_platform_output(window, full_output.platform_output);

        self.paint_jobs = self
            .context
            .tessellate(full_output.shapes, full_output.pixels_per_point);

        self.textures_delta = full_output.textures_delta;

        let size = window.inner_size();
        self.screen_descriptor = ScreenDescriptor {
            size_in_pixels: [size.width, size.height],
            pixels_per_point: window.scale_factor() as f32,
        };
    }

    /// Upload tessellated mesh data into the GPU vertex/index buffers.
    ///
    /// Must be called after [`Self::end_frame`] and [`Self::update_textures`],
    /// before [`Self::render`]. Returns user-defined command buffers from
    /// callback traits (empty for simple usage).
    pub fn update_buffers(
        &mut self,
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        encoder: &mut wgpu::CommandEncoder,
    ) -> Vec<wgpu::CommandBuffer> {
        self.renderer.update_buffers(
            device,
            queue,
            encoder,
            &self.paint_jobs,
            &self.screen_descriptor,
        )
    }

    /// Upload new texture data (glyph atlas, user images) and free stale
    /// textures based on the delta produced by [`Self::end_frame`].
    ///
    /// Must be called after [`Self::end_frame`] and before
    /// [`Self::update_buffers`].
    pub fn update_textures(
        &mut self,
        device: &wgpu::Device,
        queue: &wgpu::Queue,
    ) {
        for id in self.textures_delta.free.drain(..) {
            self.renderer.free_texture(&id);
        }
        for (id, delta) in self.textures_delta.set.drain(..) {
            self.renderer.update_texture(device, queue, id, &delta);
        }
    }

    /// Record egui draw commands into a render pass.
    ///
    /// Call after [`Self::update_buffers`]. The pass should target the
    /// final surface view with `LoadOp::Load` so the UI composites on
    /// top of the 3-D scene.
    pub fn render(&self, pass: &mut wgpu::RenderPass<'_>) {
        self.renderer.render(
            forget_render_pass_lifetime(pass),
            &self.paint_jobs,
            &self.screen_descriptor,
        );
    }

    /// Handle a DPI scale change.
    pub fn set_scale_factor(&mut self, scale_factor: f32) {
        self.screen_descriptor.pixels_per_point = scale_factor;
    }
}

//! `Gfx` — top-level wgpu context.
//!
//! Owns the `wgpu::Instance`, `Adapter`, `Device`, `Queue`, `Surface`, and the
//! `Arc<Window>` that backs the surface. Constructed once at startup from
//! [`crate::app::App`]'s `resumed` handler, and held for the lifetime of the
//! window.
//!
//! Per the migration plan §4.10, this is the foundation that all subsequent
//! [C] sub-tasks (pipelines, atlases, uniforms, text) build on.

use std::sync::Arc;

use winit::window::Window;

/// Top-level `wgpu` context.
///
/// Surface is `'static` because it borrows from the `Arc<Window>` that the
/// struct itself owns — the Arc keeps the window alive for at least as long
/// as the surface.
pub struct Gfx {
    /// The `Arc<Window>` is the first field so it outlives `surface` in drop
    /// order; `surface` borrows from it (transitively, via the Arc clone we
    /// pass to `create_surface`).
    window: Arc<Window>,
    instance: wgpu::Instance,
    adapter: wgpu::Adapter,
    device: wgpu::Device,
    queue: wgpu::Queue,
    surface: wgpu::Surface<'static>,
    surface_config: wgpu::SurfaceConfiguration,
}

impl Gfx {
    /// Build the wgpu context for `window`.
    ///
    /// Uses `pollster::block_on` to drive the async adapter/device requests
    /// synchronously — fine for one-shot startup. Picks an sRGB surface
    /// format if available, falls back to `caps.formats[0]`. Vsync (`Fifo`)
    /// is the present mode.
    #[must_use]
    pub fn new(window: Arc<Window>) -> Self {
        let size = window.inner_size();
        let width = size.width.max(1);
        let height = size.height.max(1);

        let mut instance_desc = wgpu::InstanceDescriptor::new_without_display_handle();
        instance_desc.backends = wgpu::Backends::PRIMARY;
        let instance = wgpu::Instance::new(instance_desc);

        // Surface borrows from the window via the Arc clone; the Arc keeps
        // the window alive at least as long as the surface lives, which lets
        // wgpu treat the surface as `'static`.
        let surface = instance
            .create_surface(window.clone())
            .expect("failed to create wgpu surface");

        let adapter = pollster::block_on(instance.request_adapter(&wgpu::RequestAdapterOptions {
            power_preference: wgpu::PowerPreference::HighPerformance,
            compatible_surface: Some(&surface),
            force_fallback_adapter: false,
        }))
        .expect("failed to request wgpu adapter");

        let adapter_info = adapter.get_info();
        tracing::info!(
            backend = ?adapter_info.backend,
            name = %adapter_info.name,
            device_type = ?adapter_info.device_type,
            "wgpu adapter selected"
        );

        let (device, queue) = pollster::block_on(adapter.request_device(&wgpu::DeviceDescriptor {
            label: Some("neworld.device"),
            required_features: wgpu::Features::empty(),
            required_limits: wgpu::Limits::default(),
            ..wgpu::DeviceDescriptor::default()
        }))
        .expect("failed to request wgpu device");

        let caps = surface.get_capabilities(&adapter);

        // Prefer Bgra8UnormSrgb (the C++ build also relies on sRGB
        // framebuffers via GL_FRAMEBUFFER_SRGB — see migration plan §6).
        // Fall back to the first sRGB-capable format, then to whatever the
        // surface offers first.
        let surface_format = if caps.formats.contains(&wgpu::TextureFormat::Bgra8UnormSrgb) {
            wgpu::TextureFormat::Bgra8UnormSrgb
        } else if let Some(srgb) = caps.formats.iter().copied().find(wgpu::TextureFormat::is_srgb) {
            srgb
        } else {
            caps.formats[0]
        };

        let alpha_mode = caps
            .alpha_modes
            .iter()
            .copied()
            .find(|m| *m == wgpu::CompositeAlphaMode::Auto)
            .unwrap_or(caps.alpha_modes[0]);

        let surface_config = wgpu::SurfaceConfiguration {
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
            format: surface_format,
            width,
            height,
            present_mode: wgpu::PresentMode::Fifo,
            desired_maximum_frame_latency: 2,
            alpha_mode,
            view_formats: vec![],
        };

        surface.configure(&device, &surface_config);

        tracing::info!(
            width,
            height,
            ?surface_format,
            ?alpha_mode,
            "wgpu surface configured"
        );

        Self {
            window,
            instance,
            adapter,
            device,
            queue,
            surface,
            surface_config,
        }
    }

    /// Reconfigure the surface for a new window size.
    ///
    /// Clamps both dimensions to a minimum of 1, since `wgpu` rejects
    /// zero-sized surfaces (which can happen on minimize on some platforms).
    pub fn resize(&mut self, width: u32, height: u32) {
        let width = width.max(1);
        let height = height.max(1);
        if self.surface_config.width == width && self.surface_config.height == height {
            return;
        }
        self.surface_config.width = width;
        self.surface_config.height = height;
        self.surface.configure(&self.device, &self.surface_config);
        tracing::debug!(width, height, "wgpu surface reconfigured");
    }

    /// Reconfigure the surface using the current `surface_config` (used
    /// after a `Lost`/`Outdated` acquire result).
    pub fn reconfigure(&self) {
        self.surface.configure(&self.device, &self.surface_config);
    }

    /// Acquire the next surface texture for rendering.
    ///
    /// Thin wrapper around `Surface::get_current_texture`. The caller
    /// inspects the [`wgpu::CurrentSurfaceTexture`] variant to decide
    /// whether to render, skip the frame, or reconfigure.
    pub fn acquire(&self) -> wgpu::CurrentSurfaceTexture {
        self.surface.get_current_texture()
    }

    /// The pixel format the surface was configured with.
    #[must_use]
    pub fn surface_format(&self) -> wgpu::TextureFormat {
        self.surface_config.format
    }

    /// Surface dimensions (width, height) in physical pixels.
    #[must_use]
    pub fn surface_size(&self) -> (u32, u32) {
        (self.surface_config.width, self.surface_config.height)
    }

    /// Borrow the wgpu device.
    #[must_use]
    pub fn device(&self) -> &wgpu::Device {
        &self.device
    }

    /// Borrow the wgpu queue.
    #[must_use]
    pub fn queue(&self) -> &wgpu::Queue {
        &self.queue
    }

    /// Borrow the surface configuration (read-only).
    #[must_use]
    pub fn surface_config(&self) -> &wgpu::SurfaceConfiguration {
        &self.surface_config
    }

    /// Borrow the wgpu adapter (used by later sub-tasks for capability
    /// queries).
    #[must_use]
    pub fn adapter(&self) -> &wgpu::Adapter {
        &self.adapter
    }

    /// Borrow the wgpu instance.
    #[must_use]
    pub fn instance(&self) -> &wgpu::Instance {
        &self.instance
    }

    /// Borrow the backing window (used by `app.rs` to call
    /// `request_redraw()` between frames).
    #[must_use]
    pub fn window(&self) -> &Arc<Window> {
        &self.window
    }
}

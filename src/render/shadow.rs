//! Shadow map — placeholder for the upcoming Tier 4 shadow pipeline.
//!
//! Today this owns a 1×1 `Depth32Float` texture plus a comparison sampler
//! shaped like the eventual real shadow map: the composition pass binds it
//! exactly the way it'll bind the real depth atlas, so the WGSL plumbing
//! is in place from day one. The composition shader uses
//! `FrameUniforms::shadow_params.x` (the shadow resolution) as a "shadow
//! enabled" flag — it stays at `0.0` until the shadow pipeline lands and
//! starts rendering the sun's view of the world into a properly-sized
//! depth atlas.
//!
//! Mirrors the C++ `ShadowDepthTexture` exactly: `DEPTH32F` format, linear
//! filter, `GEQUAL` (`CompareFunction::GreaterEqual`) comparison — the
//! reversed-Z equivalent of standard `LessEqual` shadow PCF. The C++ build
//! also pairs the depth attachment with a `ShadowColorTexture` (RGBA8) for
//! its framebuffer; wgpu allows depth-only render passes so that
//! attachment is intentionally not ported.
//!
//! When the shadow pass arrives it will swap [`ShadowMap::new`] for a
//! `from_resolution(device, res)` constructor, expose a `view` for use as
//! a depth attachment (in addition to the current `view` used as a binding
//! resource), and start writing `shadow_view_proj` and `shadow_params` to
//! `FrameUniforms`.

/// Owns the shadow-map texture + sampler. The names match what the
/// composition shader expects (`shadow_texture`, `shadow_sampler`).
pub struct ShadowMap {
    /// Underlying depth texture. Held to keep the view alive and so the
    /// future shadow pipeline can recreate it via [`Self::resize`].
    #[allow(dead_code)]
    pub texture: wgpu::Texture,
    /// `texture_depth_2d` view bound to the composition pipeline.
    pub view: wgpu::TextureView,
    /// `sampler_comparison` for `textureSampleCompare` calls in WGSL —
    /// matches how the C++ build uses `sampler2DArrayShadow` for soft
    /// shadow PCF taps.
    pub sampler: wgpu::Sampler,
    /// Side length of the depth texture in pixels (always square; matches
    /// the convention `ShadowRes × ShadowRes` from the C++ side).
    pub resolution: u32,
}

impl ShadowMap {
    /// Format used by the depth attachment. Shared with the depth-only
    /// chunk pass that will write to it (matches the C++ `DEPTH32F`).
    pub const FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Depth32Float;

    /// Allocate a 1×1 placeholder depth texture + comparison sampler.
    /// Adequate for "no shadow" composition — the sample call returns a
    /// constant value and the shader masks it out via `shadow_params.x`.
    pub fn new(device: &wgpu::Device) -> Self {
        Self::with_resolution(device, 1)
    }

    /// Allocate a shadow map at the requested square resolution. The
    /// shadow pipeline will call this once it knows the user-configured
    /// `Config::shadow_res`.
    pub fn with_resolution(device: &wgpu::Device, resolution: u32) -> Self {
        let resolution = resolution.max(1);
        let texture = device.create_texture(&wgpu::TextureDescriptor {
            label: Some("gfx::shadow_map.texture"),
            size: wgpu::Extent3d {
                width: resolution,
                height: resolution,
                depth_or_array_layers: 1,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: Self::FORMAT,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING,
            view_formats: &[],
        });
        let view = texture.create_view(&wgpu::TextureViewDescriptor {
            label: Some("gfx::shadow_map.view"),
            aspect: wgpu::TextureAspect::DepthOnly,
            ..Default::default()
        });
        // Comparison sampler — matches the C++ shadow texture's
        // `set_filter(true)` (linear) + `set_depth_compare_mode(GEQUAL)`.
        // GreaterEqual is the reversed-Z equivalent of standard-Z's
        // LessEqual — a point is lit (passes the test) when its depth is
        // ≥ the closest occluder's depth in the shadow map.
        let sampler = device.create_sampler(&wgpu::SamplerDescriptor {
            label: Some("gfx::shadow_map.sampler"),
            address_mode_u: wgpu::AddressMode::ClampToEdge,
            address_mode_v: wgpu::AddressMode::ClampToEdge,
            address_mode_w: wgpu::AddressMode::ClampToEdge,
            mag_filter: wgpu::FilterMode::Linear,
            min_filter: wgpu::FilterMode::Linear,
            mipmap_filter: wgpu::MipmapFilterMode::Nearest,
            compare: Some(wgpu::CompareFunction::GreaterEqual),
            lod_min_clamp: 0.0,
            lod_max_clamp: 100.0,
            ..Default::default()
        });
        Self {
            texture,
            view,
            sampler,
            resolution,
        }
    }

    /// Recreate the depth texture for a new resolution. No-op when the
    /// requested size matches the current one.
    #[allow(dead_code)] // wired into the upcoming shadow pipeline.
    pub fn resize(&mut self, device: &wgpu::Device, resolution: u32) {
        if self.resolution == resolution.max(1) {
            return;
        }
        *self = Self::with_resolution(device, resolution);
    }
}

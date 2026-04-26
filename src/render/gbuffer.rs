//! G-buffer for the deferred renderer (Tier 4).
//!
//! Mirrors the C++ `Renderer::Deferred` framebuffer in `rendering.ixx`
//! one-for-one — same formats so the composition shader can be a direct
//! port of `final.fsh`:
//!
//! | Slot     | C++ format    | Rust format       | Purpose                              |
//! |----------|---------------|-------------------|--------------------------------------|
//! | Diffuse  | `RGBA32F`     | `Rgba32Float`     | HDR albedo (alpha = translucency hint) |
//! | Normal   | `RGBA8_UNORM` | `Rgba8Unorm`      | World-space normal `(n+1)*0.5`       |
//! | Material | `RGBA8_UNORM` | `Rgba8Unorm`      | Block id encoded as 2 bytes (`hi/lo`)|
//! | Depth    | `DEPTH32F`    | `Depth32Float`    | Reversed-Z scene depth               |
//!
//! Blend state: the C++ deferred pass leaves the default GL blend in place
//! (`GL_FUNC_ADD, GL_ONE, GL_ZERO` = REPLACE) for both opaque and
//! translucent draws. Our chunk pipelines do the same — translucent water
//! survives the depth test and overwrites whatever was there, marking the
//! pixel as translucent via `diffuse.a < 1.0` so the composition pass (and
//! the future SSR / refraction pass) can treat it specially.
//!
//! The composition pass (`src/render/composition.rs`) samples all four
//! attachments via `textureLoad` (no filtering), so the textures advertise
//! `TEXTURE_BINDING` and the shaders bind them as non-filtered:
//! `texture_2d<f32>` for diffuse/normal/material, `texture_depth_2d` for
//! depth.

use crate::render::depth::DepthTarget;

/// One attached texture in the G-buffer: holds the [`wgpu::Texture`] for
/// resize / drop ownership and a default `D2` view used both as a render
/// attachment and as a fragment-shader input.
pub struct GBufferAttachment {
    pub texture: wgpu::Texture,
    pub view: wgpu::TextureView,
    pub format: wgpu::TextureFormat,
}

impl GBufferAttachment {
    fn new(
        device: &wgpu::Device,
        width: u32,
        height: u32,
        format: wgpu::TextureFormat,
        label: &str,
    ) -> Self {
        let texture = device.create_texture(&wgpu::TextureDescriptor {
            label: Some(label),
            size: wgpu::Extent3d {
                width: width.max(1),
                height: height.max(1),
                depth_or_array_layers: 1,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING,
            view_formats: &[],
        });
        let view = texture.create_view(&wgpu::TextureViewDescriptor {
            label: Some(label),
            ..Default::default()
        });
        Self {
            texture,
            view,
            format,
        }
    }
}

/// All four G-buffer attachments owned together so resize keeps them in
/// lockstep. The composition pass borrows the views; the chunk pipeline
/// writes into them via [`Self::color_attachments`].
pub struct GBuffer {
    pub diffuse: GBufferAttachment,
    pub normal: GBufferAttachment,
    pub material: GBufferAttachment,
    pub depth: DepthTarget,
    width: u32,
    height: u32,
}

impl GBuffer {
    /// Diffuse / albedo target. `Rgba32Float` to match the C++ `RGBA32F`
    /// G-buffer slot — this gives the BRDF (when it lands) headroom for HDR
    /// sun radiance without losing precision in the meantime. With our
    /// REPLACE-only blend on this attachment no `Float32Blendable` feature
    /// flag is required.
    pub const DIFFUSE_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba32Float;

    /// World-space normal target. Linear `Rgba8Unorm` since normals must NOT
    /// be gamma-encoded; the chunk shader stores `(n + 1) * 0.5`.
    pub const NORMAL_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8Unorm;

    /// Material / block-id target. Matches the C++ choice (`RGBA8_UNORM`):
    /// the chunk shader encodes a 16-bit block id as two bytes
    /// (`(hi/255, lo/255, 0, 1)`) and the composition shader decodes via
    /// `hi*256 + lo`. Using a uint format would skip the encode/decode
    /// dance but breaks parity with the C++ shader port that lands next.
    pub const MATERIAL_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8Unorm;

    /// Depth target. Mirrors [`DepthTarget::FORMAT`].
    pub const DEPTH_FORMAT: wgpu::TextureFormat = DepthTarget::FORMAT;

    /// Allocate a G-buffer at `width × height` (clamped to ≥ 1).
    #[must_use]
    pub fn new(device: &wgpu::Device, width: u32, height: u32) -> Self {
        let width = width.max(1);
        let height = height.max(1);
        Self {
            diffuse: GBufferAttachment::new(
                device,
                width,
                height,
                Self::DIFFUSE_FORMAT,
                "gfx::gbuffer.diffuse",
            ),
            normal: GBufferAttachment::new(
                device,
                width,
                height,
                Self::NORMAL_FORMAT,
                "gfx::gbuffer.normal",
            ),
            material: GBufferAttachment::new(
                device,
                width,
                height,
                Self::MATERIAL_FORMAT,
                "gfx::gbuffer.material",
            ),
            depth: DepthTarget::new(device, width, height),
            width,
            height,
        }
    }

    /// Recreate every attachment for the new size. No-op when the dimensions
    /// match (after clamping).
    pub fn resize(&mut self, device: &wgpu::Device, width: u32, height: u32) {
        let width = width.max(1);
        let height = height.max(1);
        if self.width == width && self.height == height {
            return;
        }
        *self = Self::new(device, width, height);
    }

    /// Borrow the depth view (used by the chunk MRT pass and any forward
    /// pass that still wants to depth-test against the world).
    #[must_use]
    pub fn depth_view(&self) -> &wgpu::TextureView {
        &self.depth.view
    }

    /// Build the three-target color-attachment list for the chunk MRT pass.
    /// The first call in a frame should clear; subsequent calls (e.g. the
    /// translucent pass) load.
    #[must_use]
    pub fn color_attachments(
        &self,
        load: wgpu::LoadOp<wgpu::Color>,
        store: wgpu::StoreOp,
    ) -> [Option<wgpu::RenderPassColorAttachment<'_>>; 3] {
        // The material target is uint, but `wgpu::Color { r, g, b, a }` is
        // typed as f64 — the wgpu validation layer interprets the components
        // as integers in `[0, MAX]` for uint formats, so a clear value of 0
        // works correctly.
        [
            Some(wgpu::RenderPassColorAttachment {
                view: &self.diffuse.view,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations { load, store },
            }),
            Some(wgpu::RenderPassColorAttachment {
                view: &self.normal.view,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations { load, store },
            }),
            Some(wgpu::RenderPassColorAttachment {
                view: &self.material.view,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations { load, store },
            }),
        ]
    }

    /// Current size in pixels (post-clamp).
    #[must_use]
    pub fn size(&self) -> (u32, u32) {
        (self.width, self.height)
    }
}

//! G-buffer for the deferred renderer (Tier 4) — **two parallel layers**.
//!
//! The chunk pipelines write the front-most opaque fragment to the
//! `opaque` layer and the front-most translucent fragment to the
//! `translucent` layer. All blends are `REPLACE` (no alpha mixing on
//! the GPU) so each layer cleanly stores one surface per pixel; the
//! composition shader reads both layers and blends them into the
//! surface manually. This shape is what enables future screen-space
//! refraction — composition can refract through the translucent layer
//! into the opaque one.
//!
//! Per-layer attachments:
//!
//! | Slot     | Format        | Channels                                                       |
//! |----------|---------------|----------------------------------------------------------------|
//! | diffuse  | `Rgba16Float` | rgb = albedo, a = emissive (opaque) / texel α (translucent)    |
//! | normal   | `Rg8Unorm`    | xy = octahedral-encoded world-space normal                     |
//! | material | `R16Uint`     | r = u16 atlas-layer index (texture id) — direct, no encoding   |
//! | depth    | `Depth32Float`| reversed-Z scene depth                                          |
//!
//! Basic mode allocates only `diffuse + depth` per layer; the
//! `normal` and `material` slots are `Option<...>` and stay `None`.
//! Composition's basic entry samples just diffuse + depth and pays
//! nothing for the missing slots.
//!
//! Diffuse is `Rgba16Float` (HDR-capable, half-float) so advanced
//! mode's HDR sun radiance has headroom and basic mode's `[0, 1]`
//! pre-lit colour rounds cleanly. Normal is `Rg8Unorm` octahedral
//! (8 bits per axis, no alpha channel — half the memory of a 4-byte
//! Rgba8Unorm normal target with negligible angular error). Material
//! is `R16Uint` — `texture_2d<u32>` on the shader side, `textureLoad`
//! returns the u16 directly with no encode/decode dance.

use crate::render::depth::DepthTarget;

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

/// One layer of the G-buffer (opaque or translucent). `diffuse` and
/// `depth` are always present; `normal` / `material` are advanced-only.
pub struct GBufferLayer {
    pub diffuse: GBufferAttachment,
    pub normal: Option<GBufferAttachment>,
    pub material: Option<GBufferAttachment>,
    pub depth: DepthTarget,
}

impl GBufferLayer {
    fn new(
        device: &wgpu::Device,
        width: u32,
        height: u32,
        advanced: bool,
        label_prefix: &str,
    ) -> Self {
        let diffuse = GBufferAttachment::new(
            device,
            width,
            height,
            GBuffer::DIFFUSE_FORMAT,
            &format!("{label_prefix}.diffuse"),
        );
        let depth = DepthTarget::new(device, width, height);
        let (normal, material) = if advanced {
            (
                Some(GBufferAttachment::new(
                    device,
                    width,
                    height,
                    GBuffer::NORMAL_FORMAT,
                    &format!("{label_prefix}.normal"),
                )),
                Some(GBufferAttachment::new(
                    device,
                    width,
                    height,
                    GBuffer::MATERIAL_FORMAT,
                    &format!("{label_prefix}.material"),
                )),
            )
        } else {
            (None, None)
        };
        Self {
            diffuse,
            normal,
            material,
            depth,
        }
    }

    /// Color attachments for this layer's chunk pass — 1 entry in
    /// basic mode (just diffuse), 3 in advanced (diffuse + normal +
    /// material). The translucent layer's pass clears with this; the
    /// opaque layer's pass also clears (depth-tested, REPLACE blend).
    pub fn color_attachments(
        &self,
        load: wgpu::LoadOp<wgpu::Color>,
        store: wgpu::StoreOp,
    ) -> Vec<Option<wgpu::RenderPassColorAttachment<'_>>> {
        let mut out: Vec<Option<wgpu::RenderPassColorAttachment<'_>>> = Vec::with_capacity(3);
        out.push(Some(wgpu::RenderPassColorAttachment {
            view: &self.diffuse.view,
            depth_slice: None,
            resolve_target: None,
            ops: wgpu::Operations { load, store },
        }));
        if let Some(n) = &self.normal {
            out.push(Some(wgpu::RenderPassColorAttachment {
                view: &n.view,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations { load, store },
            }));
        }
        if let Some(m) = &self.material {
            out.push(Some(wgpu::RenderPassColorAttachment {
                view: &m.view,
                depth_slice: None,
                resolve_target: None,
                ops: wgpu::Operations { load, store },
            }));
        }
        out
    }

    pub fn depth_view(&self) -> &wgpu::TextureView {
        &self.depth.view
    }
}

/// Two-layer G-buffer. Opaque + translucent layers each own their
/// own diffuse/normal/material/depth so the two passes don't touch
/// each other's storage; composition reads both.
pub struct GBuffer {
    pub opaque: GBufferLayer,
    pub translucent: GBufferLayer,
    width: u32,
    height: u32,
    advanced: bool,
}

impl GBuffer {
    pub const DIFFUSE_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba16Float;
    /// World-space normal target. `Rgba8Unorm` packs:
    /// * `r`, `g` — octahedral-encoded normal (the chunk shader
    ///   writes `oct_encode(n) * 0.5 + 0.5`; composition decodes via
    ///   `oct_decode(stored*2-1)`).
    /// * `b` — per-vertex sky-light intensity (0..1), already
    ///   smooth-lit by the CPU mesher. Composition multiplies direct
    ///   sunlight by this so cave / overhang occlusion that the
    ///   shadow map misses still attenuates the lambert term.
    /// * `a` — reserved (1.0). Kept for a future per-pixel scalar
    ///   (roughness, metallic, etc.).
    pub const NORMAL_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8Unorm;
    /// Material target — direct u16 storing the atlas-layer index of
    /// the surface texture. `R16Uint` lets the shader read the value
    /// out of `texture_2d<u32>::textureLoad` as-is, no encode/decode
    /// helpers needed.
    pub const MATERIAL_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::R16Uint;
    pub const DEPTH_FORMAT: wgpu::TextureFormat = DepthTarget::FORMAT;

    #[must_use]
    pub fn new(device: &wgpu::Device, width: u32, height: u32, advanced: bool) -> Self {
        let width = width.max(1);
        let height = height.max(1);
        Self {
            opaque: GBufferLayer::new(device, width, height, advanced, "gfx::gbuffer.opaque"),
            translucent: GBufferLayer::new(
                device,
                width,
                height,
                advanced,
                "gfx::gbuffer.translucent",
            ),
            width,
            height,
            advanced,
        }
    }

    /// Recreate every attachment in both layers for the new size,
    /// preserving the current mode. No-op when dimensions match.
    pub fn resize(&mut self, device: &wgpu::Device, width: u32, height: u32) {
        let width = width.max(1);
        let height = height.max(1);
        if self.width == width && self.height == height {
            return;
        }
        *self = Self::new(device, width, height, self.advanced);
    }

    /// Switch between basic (diffuse + depth per layer) and advanced
    /// (full per-layer MRT). Reallocates the optional slots in both
    /// layers.
    pub fn set_advanced(&mut self, device: &wgpu::Device, advanced: bool) {
        if self.advanced == advanced {
            return;
        }
        *self = Self::new(device, self.width, self.height, advanced);
    }

    #[must_use]
    pub fn is_advanced(&self) -> bool {
        self.advanced
    }

    /// Borrow the opaque depth view — used as the depth attachment
    /// for the opaque chunk pass and read as a sampled texture by
    /// the translucent chunk pass (for shader-side discard of
    /// fragments behind opaque) and by the composition pass.
    #[must_use]
    pub fn opaque_depth_view(&self) -> &wgpu::TextureView {
        self.opaque.depth_view()
    }

    /// Borrow the translucent depth view — used as the depth
    /// attachment for the translucent chunk pass (so the front-most
    /// translucent fragment wins) and read by composition.
    #[must_use]
    pub fn translucent_depth_view(&self) -> &wgpu::TextureView {
        self.translucent.depth_view()
    }

    /// Current size in pixels (post-clamp).
    #[must_use]
    pub fn size(&self) -> (u32, u32) {
        (self.width, self.height)
    }
}

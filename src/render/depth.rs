//! Depth attachment for the chunk render pipeline.
//!
//! A single `Depth32Float` texture sized to match the surface. The wrapper
//! exposes a `resize` helper that recreates the texture in place when the
//! window changes; the [`crate::render::chunk_render::ChunkPipeline`] is built
//! against [`DepthTarget::FORMAT`] so resizing never invalidates the pipeline.
//!
//! Reversed-Z is enabled at the pipeline / projection level: chunk + particle
//! pipelines both use `CompareFunction::Greater`, the world render pass clears
//! depth to 0.0, and `Camera::proj_matrix` returns a reversed-Z perspective
//! (`OPENGL_TO_WGPU_REVERSED`). The depth attachment itself is format-neutral.

/// One depth attachment, owned alongside the surface configuration.
pub struct DepthTarget {
    /// The underlying `wgpu` texture. Held to keep the view alive and to allow
    /// recreation on resize.
    pub texture: wgpu::Texture,
    /// A view of the full texture, used as the depth attachment of the chunk
    /// render pass.
    pub view: wgpu::TextureView,
    /// Format the texture was created with (always [`DepthTarget::FORMAT`]).
    pub format: wgpu::TextureFormat,
}

impl DepthTarget {
    /// Format used by the chunk pipeline's depth attachment.
    pub const FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Depth32Float;

    /// Allocate a depth texture of the given size.
    ///
    /// Both dimensions are clamped to a minimum of 1 since `wgpu` rejects
    /// zero-sized textures (which can happen during a window minimize).
    pub fn new(device: &wgpu::Device, width: u32, height: u32) -> Self {
        let width = width.max(1);
        let height = height.max(1);
        let texture = device.create_texture(&wgpu::TextureDescriptor {
            label: Some("gfx::depth_target"),
            size: wgpu::Extent3d {
                width,
                height,
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
            label: Some("gfx::depth_target.view"),
            ..Default::default()
        });
        Self {
            texture,
            view,
            format: Self::FORMAT,
        }
    }

    /// Recreate the depth texture for the new size in place. No-op if the
    /// dimensions are unchanged after clamping.
    pub fn resize(&mut self, device: &wgpu::Device, width: u32, height: u32) {
        let width = width.max(1);
        let height = height.max(1);
        let size = self.texture.size();
        if size.width == width && size.height == height {
            return;
        }
        *self = Self::new(device, width, height);
    }
}

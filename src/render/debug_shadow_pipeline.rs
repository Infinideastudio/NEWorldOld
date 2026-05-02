//! Shadow-map debug overlay — host pipeline for `debug_shadow.wgsl`.
//!
//! Mirrors the C++ `DebugShadowShader` overlay (`neworld.ixx::1034`):
//! when the user toggles **F3+M** with advanced rendering on, the
//! shadow depth atlas is drawn into the top-right corner of the screen
//! as a square overlay (`xi = 1 - h/w` on the left edge, full height).
//! A binary search over `textureSampleCompare` recovers the stored depth
//! per fragment so the user can eyeball whether the sun-POV pass is
//! capturing the visible world.
//!
//! Bind-group layout:
//!   * Group 0 — shadow texture + comparison sampler. Same shape as
//!     composition's aux group entries 0–1 (depth + comparison sampler),
//!     but a separate layout instance because the debug pipeline uses
//!     only those two bindings.
//!   * Group 1 — `DebugShadowUniforms { quad: vec4 }` with the NDC quad
//!     bounds. Updated each frame from the live surface size.
//!
//! No vertex buffer: the shader synthesises the quad from
//! `@builtin(vertex_index)` (6 indices, two CCW triangles).

use bytemuck::{Pod, Zeroable};

use crate::render::gbuffer::GBuffer;
use crate::render::shadow::ShadowMap;
use crate::render::uniforms::UniformBuffer;

const SHADER_SRC: &str = include_str!("../../shaders/debug_shadow.wgsl");

/// Mirrors `DebugShadowUniforms` in `debug_shadow.wgsl`.
///
/// `quad` is `(xi, yi, xa, ya)` in NDC: top-left at `(xi, yi)`,
/// bottom-right at `(xa, ya)`. The padding scalar keeps the struct's
/// size at 16 bytes (a single `vec4` already satisfies the WGSL uniform
/// 16-byte alignment, but `#[repr(C)]` lets us be explicit).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Pod, Zeroable)]
pub struct DebugShadowUniforms {
    pub quad: [f32; 4],
}

/// Debug-shadow overlay pipeline + its bind groups.
pub struct DebugShadowPipeline {
    pipeline: wgpu::RenderPipeline,
    /// Group 0 layout — recreated when the shadow texture is resized so
    /// we can rebuild the bind group with the fresh view.
    shadow_layout: wgpu::BindGroupLayout,
    shadow_bind_group: wgpu::BindGroup,
    /// Group 1 layout — for the debug uniforms buffer. Stable.
    #[allow(dead_code)]
    uniforms_layout: wgpu::BindGroupLayout,
    uniforms_bind_group: wgpu::BindGroup,
    uniforms: UniformBuffer<DebugShadowUniforms>,
}

impl DebugShadowPipeline {
    /// Build the overlay pipeline.
    ///
    /// `surface_format` must match the surface the overlay is drawn into
    /// (typically `Bgra8UnormSrgb`). Uses `SrcAlpha / OneMinusSrcAlpha`
    /// blending so the empty-shadow-map fallback (`vec4(0.2, 0.2, 0.2,
    /// 0.5)` from the fragment shader) is semi-transparent — matches the
    /// C++ `debug_shadow.fsh` behaviour.
    pub fn new(
        device: &wgpu::Device,
        surface_format: wgpu::TextureFormat,
        shadow_map: &ShadowMap,
    ) -> Self {
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("gfx::debug_shadow.shader"),
            source: wgpu::ShaderSource::Wgsl(SHADER_SRC.into()),
        });

        let shadow_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("gfx::debug_shadow.shadow_layout"),
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Depth,
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Comparison),
                    count: None,
                },
            ],
        });

        let uniforms_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("gfx::debug_shadow.uniforms_layout"),
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                visibility: wgpu::ShaderStages::VERTEX,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: false,
                    min_binding_size: None,
                },
                count: None,
            }],
        });

        let shadow_bind_group = Self::build_shadow_bind_group(device, &shadow_layout, shadow_map);

        let uniforms =
            UniformBuffer::<DebugShadowUniforms>::new(device, "gfx::debug_shadow.uniforms");
        let uniforms_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("gfx::debug_shadow.uniforms_bg"),
            layout: &uniforms_layout,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: uniforms.binding(),
            }],
        });

        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("gfx::debug_shadow.pipeline_layout"),
            bind_group_layouts: &[Some(&shadow_layout), Some(&uniforms_layout)],
            immediate_size: 0,
        });

        let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("gfx::debug_shadow.pipeline"),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: Some("vs_main"),
                compilation_options: wgpu::PipelineCompilationOptions::default(),
                buffers: &[],
            },
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: wgpu::FrontFace::Ccw,
                cull_mode: None,
                unclipped_depth: false,
                polygon_mode: wgpu::PolygonMode::Fill,
                conservative: false,
            },
            // Pipeline must declare a depth-stencil state matching the
            // forward-overlay pass's depth attachment (G-buffer depth,
            // `Depth32Float`) — wgpu validates pipeline targets against
            // pass attachments at draw time. The overlay doesn't actually
            // test or write depth: `Always` compare + write disabled
            // means every fragment passes regardless of what's already in
            // the buffer, and the depth buffer is left untouched.
            depth_stencil: Some(wgpu::DepthStencilState {
                format: GBuffer::DEPTH_FORMAT,
                depth_write_enabled: Some(false),
                depth_compare: Some(wgpu::CompareFunction::Always),
                stencil: wgpu::StencilState::default(),
                bias: wgpu::DepthBiasState::default(),
            }),
            multisample: wgpu::MultisampleState {
                count: 1,
                mask: !0,
                alpha_to_coverage_enabled: false,
            },
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: Some("fs_main"),
                compilation_options: wgpu::PipelineCompilationOptions::default(),
                targets: &[Some(wgpu::ColorTargetState {
                    format: surface_format,
                    blend: Some(wgpu::BlendState::ALPHA_BLENDING),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            multiview_mask: None,
            cache: None,
        });

        Self {
            pipeline,
            shadow_layout,
            shadow_bind_group,
            uniforms_layout,
            uniforms_bind_group,
            uniforms,
        }
    }

    /// Recreate the shadow bind group after the underlying shadow texture
    /// view changed (i.e. after `ShadowMap::resize`). Called from
    /// `Game::apply_shadow_config`.
    pub fn rebuild_shadow_bind_group(&mut self, device: &wgpu::Device, shadow_map: &ShadowMap) {
        self.shadow_bind_group =
            Self::build_shadow_bind_group(device, &self.shadow_layout, shadow_map);
    }

    /// Update the NDC quad rectangle from the live surface size. The
    /// overlay is a square in the top-right whose side equals the screen
    /// height — same shape as C++ `xi = 1 - h/w, yi = 1, xa = 1, ya = 0`.
    pub fn update_layout(&self, queue: &wgpu::Queue, surface_size: (u32, u32)) {
        let (w, h) = (surface_size.0.max(1) as f32, surface_size.1.max(1) as f32);
        let xi = 1.0 - h / w;
        let quad = DebugShadowUniforms {
            quad: [xi, 1.0, 1.0, 0.0],
        };
        self.uniforms.write(queue, &quad);
    }

    /// Record the overlay draw — six indices, no vertex buffer. The
    /// caller opens the render pass with the surface as the only color
    /// attachment.
    pub fn draw<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>) {
        pass.set_pipeline(&self.pipeline);
        pass.set_bind_group(0, &self.shadow_bind_group, &[]);
        pass.set_bind_group(1, &self.uniforms_bind_group, &[]);
        pass.draw(0..6, 0..1);
    }

    fn build_shadow_bind_group(
        device: &wgpu::Device,
        layout: &wgpu::BindGroupLayout,
        shadow_map: &ShadowMap,
    ) -> wgpu::BindGroup {
        device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("gfx::debug_shadow.shadow_bg"),
            layout,
            entries: &[
                wgpu::BindGroupEntry {
                    binding: 0,
                    resource: wgpu::BindingResource::TextureView(&shadow_map.view),
                },
                wgpu::BindGroupEntry {
                    binding: 1,
                    resource: wgpu::BindingResource::Sampler(&shadow_map.sampler),
                },
            ],
        })
    }
}

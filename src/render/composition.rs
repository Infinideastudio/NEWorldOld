//! Deferred composition pass — port of the C++ `final.fsh` skeleton.
//!
//! Reads the G-buffer (diffuse / normal / material / depth), applies
//! sun directional lighting + ambient sky + distance fog, and writes the
//! lit color to the surface. Tier 4 follow-ons (shadow map, SSR,
//! volumetric clouds) plug into the bind groups already declared here so
//! the pipeline doesn't need rebuilding when each lands.
//!
//! Bind-group layout:
//! * Group 0: per-frame uniforms (camera, sun, screen, fog, shadow params).
//! * Group 1: G-buffer (4 bindings).
//! * Group 2: shadow texture / comparison sampler + noise texture / sampler
//!   — used for shadow PCF and SSR dither in future steps.
//!
//! No vertex buffer: the vertex shader synthesises a full-screen triangle
//! pair from `@builtin(vertex_index)` (6 indices, two CCW triangles).

use crate::render::gbuffer::GBuffer;
use crate::render::shadow::ShadowMap;
use crate::render::uniforms::{FrameUniforms, UniformBuffer};
use crate::textures::Atlases;

const SHADER_SRC: &str = include_str!("../../shaders/composition.wgsl");

/// Deferred composition pipeline + the three bind groups that feed it.
///
/// The G-buffer + shadow bind groups are recreated on resize (see
/// [`Self::rebuild_gbuffer_bind_group`]); the frame and aux bind groups
/// stay constant for the pipeline's lifetime.
pub struct CompositionPipeline {
    pipeline: wgpu::RenderPipeline,
    /// Group 0 — frame uniforms.
    frame_layout: wgpu::BindGroupLayout,
    frame_bind_group: wgpu::BindGroup,
    /// Group 1 — G-buffer attachments. Recreated on resize since the
    /// underlying texture views are recreated then.
    gbuffer_layout: wgpu::BindGroupLayout,
    gbuffer_bind_group: wgpu::BindGroup,
    /// Group 2 — shadow map + noise. Stable across the pipeline lifetime
    /// for now (shadow placeholder is 1×1; noise atlas is fixed).
    aux_layout: wgpu::BindGroupLayout,
    aux_bind_group: wgpu::BindGroup,
}

impl CompositionPipeline {
    /// Build the composition pipeline.
    ///
    /// `surface_format` is the format of the wgpu surface being written
    /// to (e.g. `Bgra8UnormSrgb`).
    #[must_use]
    pub fn new(
        device: &wgpu::Device,
        surface_format: wgpu::TextureFormat,
        frame_uniforms: &UniformBuffer<FrameUniforms>,
        gbuffer: &GBuffer,
        shadow_map: &ShadowMap,
        atlases: &Atlases,
    ) -> Self {
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("gfx::composition.shader"),
            source: wgpu::ShaderSource::Wgsl(SHADER_SRC.into()),
        });

        // ---- group 0 : frame uniforms ----
        let frame_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("gfx::composition.frame_layout"),
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                visibility: wgpu::ShaderStages::VERTEX | wgpu::ShaderStages::FRAGMENT,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: false,
                    min_binding_size: None,
                },
                count: None,
            }],
        });
        let frame_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("gfx::composition.frame_bg"),
            layout: &frame_layout,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: frame_uniforms.binding(),
            }],
        });

        // ---- group 1 : G-buffer ----
        let gbuffer_layout = Self::build_gbuffer_layout(device);
        let gbuffer_bind_group = Self::build_gbuffer_bind_group(device, &gbuffer_layout, gbuffer);

        // ---- group 2 : shadow + noise ----
        let aux_layout = Self::build_aux_layout(device);
        let aux_bind_group =
            Self::build_aux_bind_group(device, &aux_layout, shadow_map, atlases);

        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("gfx::composition.pipeline_layout"),
            bind_group_layouts: &[
                Some(&frame_layout),
                Some(&gbuffer_layout),
                Some(&aux_layout),
            ],
            immediate_size: 0,
        });

        let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("gfx::composition.pipeline"),
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
            // No depth attachment — the composition pass writes the surface
            // directly. Subsequent forward passes (particles, selection,
            // underwater) attach the G-buffer's depth in their own pass to
            // depth-test against the world.
            depth_stencil: None,
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
                    blend: Some(wgpu::BlendState::REPLACE),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            multiview_mask: None,
            cache: None,
        });

        Self {
            pipeline,
            frame_layout,
            frame_bind_group,
            gbuffer_layout,
            gbuffer_bind_group,
            aux_layout,
            aux_bind_group,
        }
    }

    /// Recreate the G-buffer bind group after the underlying texture views
    /// have been recreated by [`GBuffer::resize`]. Call from `Game::resize`.
    pub fn rebuild_gbuffer_bind_group(&mut self, device: &wgpu::Device, gbuffer: &GBuffer) {
        self.gbuffer_bind_group =
            Self::build_gbuffer_bind_group(device, &self.gbuffer_layout, gbuffer);
    }

    /// Record the composition pass: full-screen triangle pair, no vertex
    /// or index buffer. Caller is responsible for opening and closing the
    /// render pass on the surface view.
    pub fn record<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>) {
        pass.set_pipeline(&self.pipeline);
        pass.set_bind_group(0, &self.frame_bind_group, &[]);
        pass.set_bind_group(1, &self.gbuffer_bind_group, &[]);
        pass.set_bind_group(2, &self.aux_bind_group, &[]);
        pass.draw(0..6, 0..1);
    }

    fn build_gbuffer_layout(device: &wgpu::Device) -> wgpu::BindGroupLayout {
        // The G-buffer is sampled with `textureLoad` (no filter), so all four
        // bindings are non-filtering / unfiltered-uint / depth as appropriate.
        device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("gfx::composition.gbuffer_layout"),
            entries: &[
                // diffuse — Rgba8UnormSrgb, sampled as float (non-filtering OK).
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: false },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                // normal — Rgba8Unorm, sampled as float.
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: false },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                // material — Rgba8Unorm (R/G hold encoded u16). textureLoad
                // returns a `vec4<f32>` and the shader decodes via
                // `decode_u16(rg) = hi*256 + lo` (matches C++ `final.fsh`).
                wgpu::BindGroupLayoutEntry {
                    binding: 2,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: false },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                // depth — Depth32Float.
                wgpu::BindGroupLayoutEntry {
                    binding: 3,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Depth,
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
            ],
        })
    }

    fn build_gbuffer_bind_group(
        device: &wgpu::Device,
        layout: &wgpu::BindGroupLayout,
        gbuffer: &GBuffer,
    ) -> wgpu::BindGroup {
        device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("gfx::composition.gbuffer_bg"),
            layout,
            entries: &[
                wgpu::BindGroupEntry {
                    binding: 0,
                    resource: wgpu::BindingResource::TextureView(&gbuffer.diffuse.view),
                },
                wgpu::BindGroupEntry {
                    binding: 1,
                    resource: wgpu::BindingResource::TextureView(&gbuffer.normal.view),
                },
                wgpu::BindGroupEntry {
                    binding: 2,
                    resource: wgpu::BindingResource::TextureView(&gbuffer.material.view),
                },
                wgpu::BindGroupEntry {
                    binding: 3,
                    resource: wgpu::BindingResource::TextureView(gbuffer.depth_view()),
                },
            ],
        })
    }

    fn build_aux_layout(device: &wgpu::Device) -> wgpu::BindGroupLayout {
        device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("gfx::composition.aux_layout"),
            entries: &[
                // shadow_texture — depth, comparison-sampled.
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
                // shadow_sampler — comparison sampler.
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Comparison),
                    count: None,
                },
                // noise_texture — Rgba8Unorm, filtering.
                wgpu::BindGroupLayoutEntry {
                    binding: 2,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: true },
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                },
                // noise_sampler — filtering.
                wgpu::BindGroupLayoutEntry {
                    binding: 3,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                    count: None,
                },
            ],
        })
    }

    fn build_aux_bind_group(
        device: &wgpu::Device,
        layout: &wgpu::BindGroupLayout,
        shadow_map: &ShadowMap,
        atlases: &Atlases,
    ) -> wgpu::BindGroup {
        device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("gfx::composition.aux_bg"),
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
                wgpu::BindGroupEntry {
                    binding: 2,
                    resource: wgpu::BindingResource::TextureView(&atlases.block_noise.view),
                },
                wgpu::BindGroupEntry {
                    binding: 3,
                    // Linear + Repeat sampler dedicated to the noise
                    // texture — mirrors the C++ `set_filter(true, false)`
                    // / `set_wrap(true)` on `NoiseTextureArray`.
                    resource: wgpu::BindingResource::Sampler(atlases.noise_sampler()),
                },
            ],
        })
    }

    /// Borrow the frame bind-group layout — kept around for the future
    /// shadow pass which will want to share it.
    #[must_use]
    pub fn frame_bind_group_layout(&self) -> &wgpu::BindGroupLayout {
        &self.frame_layout
    }

    /// Borrow the aux bind-group layout — kept around for forward passes
    /// that may want to bind the shadow map directly later.
    #[must_use]
    pub fn aux_bind_group_layout(&self) -> &wgpu::BindGroupLayout {
        &self.aux_layout
    }
}

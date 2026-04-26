//! Sun-POV depth-only chunk pass — host pipeline for `shadow.wgsl`.
//!
//! Renders the same opaque chunk geometry as the G-buffer pass, but from
//! the sun's point of view, into [`ShadowMap`]'s depth attachment. The
//! resulting depth atlas is sampled by the composition pass via
//! `textureSampleCompare` for soft-shadow PCF; today the composition shader
//! gates that on `FrameUniforms::shadow_params.x > 0`, so this pipeline
//! actually starting to write data is what flips shadows on at runtime.
//!
//! Mirrors C++ `Renderer::ShadowShader` + `framebuffers[Shadow]` from
//! `rendering.ixx`:
//!
//! * Reuses the chunk vertex layout — the shader ignores `face` and `light`
//!   but reads `block_id` (leaf-wave animation) and `uv` / `layer` (alpha
//!   discard for non-rectangular blocks).
//! * Reuses the chunk pipeline's bind-group layouts so vertex buffers from
//!   `ChunkMesh` can be drawn through this pipeline without rebinding the
//!   atlas. Group 0 = frame uniforms (with `shadow_view_proj` populated by
//!   the host); group 1 = block_diffuse + sampler (alpha test only).
//! * Depth-only output: wgpu allows render passes with no color attachments,
//!   so the C++ `ShadowColorTexture` (RGBA8) is intentionally not ported.
//! * Reversed-Z: depth attachment cleared to `0.0` (= far plane), depth
//!   compare `Greater`, depth-write enabled. Pairs with the ShadowMap's
//!   comparison sampler (`GreaterEqual`).
//! * No back-face culling — same as C++ `glDisable(GL_CULL_FACE)` in
//!   `StartShadowPass`. Letting both faces write depth keeps thin sloped
//!   geometry from acne'ing under reversed-Z PCF.

use crate::render::shadow::ShadowMap;
use crate::render::uniforms::{FrameUniforms, UniformBuffer};
use crate::textures::Atlases;
use crate::worlds::chunk_rendering::{ChunkMesh, vertex_stride};

const SHADER_SRC: &str = include_str!("../../shaders/shadow.wgsl");

/// Depth-only chunk pipeline for the shadow pass.
///
/// Holds its own bind groups (frame uniforms + atlas) — separate instances
/// from the chunk pipeline because the bind-group layout used at pipeline
/// creation must match the layout the bind groups were created against, and
/// cloning a `BindGroupLayout` is not free in wgpu's validation model. The
/// underlying GPU resources (uniform buffer + atlas texture / sampler) are
/// shared via `&` references.
pub struct ShadowPipeline {
    pipeline: wgpu::RenderPipeline,
    /// Group 0 (frame uniforms).
    #[allow(dead_code)]
    frame_layout: wgpu::BindGroupLayout,
    /// Group 1 (atlas + sampler). The fragment shader only alpha-tests
    /// against the diffuse texture; the sampler stays filtering.
    #[allow(dead_code)]
    atlas_layout: wgpu::BindGroupLayout,
    frame_bind_group: wgpu::BindGroup,
    atlas_bind_group: wgpu::BindGroup,
}

impl ShadowPipeline {
    /// Build the depth-only chunk pipeline + its bind groups.
    #[must_use]
    pub fn new(
        device: &wgpu::Device,
        frame_uniforms: &UniformBuffer<FrameUniforms>,
        atlases: &Atlases,
    ) -> Self {
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("gfx::shadow_pipeline.shader"),
            source: wgpu::ShaderSource::Wgsl(SHADER_SRC.into()),
        });

        let frame_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("gfx::shadow_pipeline.frame_layout"),
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

        let atlas_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("gfx::shadow_pipeline.atlas_layout"),
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: true },
                        view_dimension: wgpu::TextureViewDimension::D2Array,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                    count: None,
                },
            ],
        });

        let frame_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("gfx::shadow_pipeline.frame_bg"),
            layout: &frame_layout,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: frame_uniforms.binding(),
            }],
        });

        let atlas_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("gfx::shadow_pipeline.atlas_bg"),
            layout: &atlas_layout,
            entries: &[
                wgpu::BindGroupEntry {
                    binding: 0,
                    resource: wgpu::BindingResource::TextureView(&atlases.block_diffuse.view),
                },
                wgpu::BindGroupEntry {
                    binding: 1,
                    resource: wgpu::BindingResource::Sampler(atlases.sampler()),
                },
            ],
        });

        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("gfx::shadow_pipeline.pipeline_layout"),
            bind_group_layouts: &[Some(&frame_layout), Some(&atlas_layout)],
            immediate_size: 0,
        });

        // Vertex layout — must match `ChunkVertex` byte-for-byte. Same six
        // attributes as the chunk pipeline; the shadow shader ignores
        // `face` / `light` but the host layout must still describe them.
        let vertex_attributes = [
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Float32x3,
                offset: 0,
                shader_location: 0,
            },
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Float32x2,
                offset: 12,
                shader_location: 1,
            },
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Uint32,
                offset: 20,
                shader_location: 2,
            },
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Uint32,
                offset: 24,
                shader_location: 3,
            },
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Uint32,
                offset: 28,
                shader_location: 4,
            },
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Uint32,
                offset: 32,
                shader_location: 5,
            },
        ];

        let vertex_buffers = [wgpu::VertexBufferLayout {
            array_stride: vertex_stride(),
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &vertex_attributes,
        }];

        // Reversed-Z: depth-write enabled, `Greater` compare. Clear value
        // 0.0 (= far) is set on the render pass each frame.
        let depth_state = wgpu::DepthStencilState {
            format: ShadowMap::FORMAT,
            depth_write_enabled: Some(true),
            depth_compare: Some(wgpu::CompareFunction::Greater),
            stencil: wgpu::StencilState::default(),
            bias: wgpu::DepthBiasState::default(),
        };

        let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("gfx::shadow_pipeline.pipeline"),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: Some("vs_main"),
                compilation_options: wgpu::PipelineCompilationOptions::default(),
                buffers: &vertex_buffers,
            },
            // No culling — matches C++ `glDisable(GL_CULL_FACE)` in
            // `StartShadowPass`. With reversed-Z + `Greater` compare the
            // closest fragment wins regardless of facing direction; letting
            // both sides write depth avoids holes at thin sloped geometry.
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: wgpu::FrontFace::Ccw,
                cull_mode: None,
                unclipped_depth: false,
                polygon_mode: wgpu::PolygonMode::Fill,
                conservative: false,
            },
            depth_stencil: Some(depth_state),
            multisample: wgpu::MultisampleState {
                count: 1,
                mask: !0,
                alpha_to_coverage_enabled: false,
            },
            // Depth-only render — no color targets. The shader's `fs_main`
            // returns `()` and only controls alpha-discard / depth write.
            fragment: Some(wgpu::FragmentState {
                module: &shader,
                entry_point: Some("fs_main"),
                compilation_options: wgpu::PipelineCompilationOptions::default(),
                targets: &[],
            }),
            multiview_mask: None,
            cache: None,
        });

        Self {
            pipeline,
            frame_layout,
            atlas_layout,
            frame_bind_group,
            atlas_bind_group,
        }
    }

    /// Record the shadow pass into `encoder`: clear `shadow_map.view` to
    /// 0.0 (reversed-Z far), bind the pipeline + groups, and re-issue every
    /// loaded chunk's opaque mesh from the sun's POV. Translucent chunks
    /// are intentionally skipped — water / glass / leaves shouldn't cast
    /// hard shadows (matches C++ which only ran the opaque path through
    /// the shadow shader).
    pub fn record<'a>(
        &'a self,
        encoder: &mut wgpu::CommandEncoder,
        shadow_map: &ShadowMap,
        meshes: impl IntoIterator<Item = &'a ChunkMesh>,
    ) {
        let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
            label: Some("game.shadow_pass"),
            color_attachments: &[],
            depth_stencil_attachment: Some(wgpu::RenderPassDepthStencilAttachment {
                view: &shadow_map.view,
                depth_ops: Some(wgpu::Operations {
                    // Reversed-Z far plane = 0.0; closest occluder writes
                    // the largest value, comparison sampler returns "lit"
                    // when the test point is at or past that value.
                    load: wgpu::LoadOp::Clear(0.0),
                    store: wgpu::StoreOp::Store,
                }),
                stencil_ops: None,
            }),
            timestamp_writes: None,
            occlusion_query_set: None,
            multiview_mask: None,
        });
        pass.set_pipeline(&self.pipeline);
        pass.set_bind_group(0, &self.frame_bind_group, &[]);
        pass.set_bind_group(1, &self.atlas_bind_group, &[]);
        for cm in meshes {
            cm.draw_opaque(&mut pass);
        }
    }
}

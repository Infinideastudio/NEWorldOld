//! Full-screen underwater overlay — drawn after the world pass when the
//! player's eye sits inside a water block.
//!
//! Mirrors `neworld.ixx:783-799` from the C++ build: a fullscreen quad
//! sampled from the water face of the block diffuse atlas, alpha-blended
//! over the surface. The water texture's per-pixel alpha provides the
//! tinted-glass look (no constant tint of our own).

use bytemuck::{Pod, Zeroable};
use wgpu::util::DeviceExt;

use crate::textures::Atlases;

const SHADER_SRC: &str = include_str!("../../shaders/underwater.wgsl");

/// Uniform pushed once per frame: the water atlas layer + an enable
/// toggle. The shader collapses the quad to a degenerate point when
/// `enabled == 0`, so the draw is cheap on the dry-player path.
#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
struct OverlayUniforms {
    layer: u32,
    enabled: u32,
    _pad0: u32,
    _pad1: u32,
}

/// GPU resources for the underwater overlay.
pub struct UnderwaterPipeline {
    pipeline: wgpu::RenderPipeline,
    atlas_bg: wgpu::BindGroup,
    overlay_bg: wgpu::BindGroup,
    overlay_buffer: wgpu::Buffer,
    /// Atlas layer index of the water top face, resolved once at construction.
    /// Stored as an `Option` so the registry lookup can fail gracefully on a
    /// stripped-down test setup.
    water_layer: u32,
    /// Tracks the last `enabled` flag we wrote so we don't re-upload every
    /// frame when the player stays dry / stays submerged.
    last_enabled: bool,
}

impl UnderwaterPipeline {
    /// Compile the pipeline. `water_layer` is the atlas index sampled when
    /// the overlay is active; pass [`crate::blocks::BlockInfo::face`]`(0).0`
    /// for the water block. `depth_format` must match whatever depth
    /// attachment the world render pass uses — wgpu rejects pipelines
    /// whose depth-stencil format disagrees with the pass even when the
    /// pipeline never reads or writes the buffer.
    pub fn new(
        device: &wgpu::Device,
        color_format: wgpu::TextureFormat,
        depth_format: wgpu::TextureFormat,
        atlases: &Atlases,
        water_layer: u32,
    ) -> Self {
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("underwater_pipeline shader"),
            source: wgpu::ShaderSource::Wgsl(SHADER_SRC.into()),
        });

        let atlas_bgl = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("underwater_pipeline atlas_bgl"),
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

        let overlay_bgl = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("underwater_pipeline overlay_bgl"),
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

        let atlas_bg = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("underwater_pipeline atlas_bg"),
            layout: &atlas_bgl,
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

        let overlay_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("underwater_pipeline overlay_uniform"),
            contents: bytemuck::bytes_of(&OverlayUniforms {
                layer: water_layer,
                enabled: 0,
                _pad0: 0,
                _pad1: 0,
            }),
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        });

        let overlay_bg = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("underwater_pipeline overlay_bg"),
            layout: &overlay_bgl,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: overlay_buffer.as_entire_binding(),
            }],
        });

        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("underwater_pipeline layout"),
            bind_group_layouts: &[Some(&atlas_bgl), Some(&overlay_bgl)],
            immediate_size: 0,
        });

        let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("underwater_pipeline"),
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
            // Overlay sits above everything; `Always` makes the depth test
            // pass unconditionally, `depth_write_enabled = false` keeps the
            // buffer untouched. The format still has to match the pass's
            // depth attachment or wgpu refuses the bind.
            depth_stencil: Some(wgpu::DepthStencilState {
                format: depth_format,
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
                    format: color_format,
                    blend: Some(wgpu::BlendState::ALPHA_BLENDING),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            multiview_mask: None,
            cache: None,
        });

        Self {
            pipeline,
            atlas_bg,
            overlay_bg,
            overlay_buffer,
            water_layer,
            last_enabled: false,
        }
    }

    /// Toggle the overlay. `true` activates the water tint on the next
    /// [`Self::draw`]; `false` deactivates it (the shader collapses the
    /// quad to a degenerate point so submitting the draw is essentially
    /// free). Cheap on the no-op path — only re-uploads when state flips.
    pub fn set_enabled(&mut self, queue: &wgpu::Queue, enabled: bool) {
        if enabled == self.last_enabled {
            return;
        }
        let u = OverlayUniforms {
            layer: self.water_layer,
            enabled: u32::from(enabled),
            _pad0: 0,
            _pad1: 0,
        };
        queue.write_buffer(&self.overlay_buffer, 0, bytemuck::bytes_of(&u));
        self.last_enabled = enabled;
    }

    /// Issue the overlay draw inside an already-bound color pass. Always
    /// emits a 6-vertex draw; the shader returns early when the overlay is
    /// disabled (collapse to a degenerate point).
    pub fn draw<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>) {
        if !self.last_enabled {
            return;
        }
        pass.set_pipeline(&self.pipeline);
        pass.set_bind_group(0, &self.atlas_bg, &[]);
        pass.set_bind_group(1, &self.overlay_bg, &[]);
        pass.draw(0..6, 0..1);
    }
}

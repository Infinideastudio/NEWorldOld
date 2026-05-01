//! Deferred composition pipeline — two-layer manual compose.
//!
//! Both modes share one `CompositionPipeline`; basic / advanced
//! variants of the pipeline + bind groups live side-by-side and the
//! active mode is selected at record time via `record(pass, advanced)`.
//!
//! Bind-group structure (mirrors `shaders/composition.wgsl`):
//!
//! * group 0 — frame uniforms.
//! * group 1 — opaque G-buffer layer (basic: 2 bindings, advanced: 4).
//! * group 2 — translucent G-buffer layer (basic: 2, advanced: 4).
//! * group 3 — advanced aux (shadow + noise). Built but unused in
//!   basic mode — its bind group is shared between rebuilds.
//!
//! Both layers always exist (the chunk pipelines need them as render
//! targets), so the per-layer bind groups are rebuilt together
//! whenever the G-buffer is recreated by resize / mode toggle.

use crate::render::gbuffer::GBuffer;
use crate::render::shadow::ShadowMap;
use crate::render::uniforms::{FrameUniforms, UniformBuffer};
use crate::textures::Atlases;

const SHADER_SRC: &str = include_str!("../../shaders/composition.wgsl");

/// Boolean feature flags for the advanced composition shader. naga
/// folds them at pipeline build-time and DCEs the disabled branches.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CompositionFeatures {
    pub soft_shadow: bool,
    pub volumetric_clouds: bool,
    pub ambient_occlusion: bool,
}

impl CompositionFeatures {
    fn as_constants(&self) -> [(&'static str, f64); 3] {
        [
            ("soft_shadow", if self.soft_shadow { 1.0 } else { 0.0 }),
            (
                "volumetric_clouds",
                if self.volumetric_clouds { 1.0 } else { 0.0 },
            ),
            (
                "ambient_occlusion",
                if self.ambient_occlusion { 1.0 } else { 0.0 },
            ),
        ]
    }
}

pub struct CompositionPipeline {
    surface_format: wgpu::TextureFormat,
    features: CompositionFeatures,
    shader: wgpu::ShaderModule,

    // ---- shared ----
    frame_layout: wgpu::BindGroupLayout,
    frame_bind_group: wgpu::BindGroup,

    // ---- basic ----
    basic_pipeline: wgpu::RenderPipeline,
    /// Held for symmetry with `advanced_pipeline_layout` — the basic
    /// pipeline has no feature flags so we never rebuild it.
    #[allow(dead_code)]
    basic_pipeline_layout: wgpu::PipelineLayout,
    basic_layer_layout: wgpu::BindGroupLayout,
    basic_opaque_bg: wgpu::BindGroup,
    basic_translucent_bg: wgpu::BindGroup,

    // ---- advanced ----
    advanced_pipeline: wgpu::RenderPipeline,
    advanced_pipeline_layout: wgpu::PipelineLayout,
    advanced_layer_layout: wgpu::BindGroupLayout,
    advanced_opaque_bg: Option<wgpu::BindGroup>,
    advanced_translucent_bg: Option<wgpu::BindGroup>,
    advanced_aux_layout: wgpu::BindGroupLayout,
    advanced_aux_bind_group: wgpu::BindGroup,
}

impl CompositionPipeline {
    #[must_use]
    pub fn new(
        device: &wgpu::Device,
        surface_format: wgpu::TextureFormat,
        frame_uniforms: &UniformBuffer<FrameUniforms>,
        gbuffer: &GBuffer,
        shadow_map: &ShadowMap,
        atlases: &Atlases,
        features: CompositionFeatures,
    ) -> Self {
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("gfx::composition.shader"),
            source: wgpu::ShaderSource::Wgsl(SHADER_SRC.into()),
        });

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

        let basic_layer_layout = build_basic_layer_layout(device);
        let basic_opaque_bg =
            build_basic_layer_bg(device, &basic_layer_layout, &gbuffer.opaque, "opaque");
        let basic_translucent_bg = build_basic_layer_bg(
            device,
            &basic_layer_layout,
            &gbuffer.translucent,
            "translucent",
        );

        let advanced_layer_layout = build_advanced_layer_layout(device);
        let advanced_opaque_bg = if gbuffer.is_advanced() {
            Some(build_advanced_layer_bg(
                device,
                &advanced_layer_layout,
                &gbuffer.opaque,
                "opaque",
            ))
        } else {
            None
        };
        let advanced_translucent_bg = if gbuffer.is_advanced() {
            Some(build_advanced_layer_bg(
                device,
                &advanced_layer_layout,
                &gbuffer.translucent,
                "translucent",
            ))
        } else {
            None
        };

        let advanced_aux_layout = build_advanced_aux_layout(device);
        let advanced_aux_bind_group = build_advanced_aux_bind_group(
            device,
            &advanced_aux_layout,
            shadow_map,
            atlases,
        );

        let basic_pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("gfx::composition.basic_pipeline_layout"),
            bind_group_layouts: &[
                Some(&frame_layout),
                Some(&basic_layer_layout),
                Some(&basic_layer_layout),
            ],
            immediate_size: 0,
        });
        let advanced_pipeline_layout =
            device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
                label: Some("gfx::composition.advanced_pipeline_layout"),
                bind_group_layouts: &[
                    Some(&frame_layout),
                    Some(&advanced_layer_layout),
                    Some(&advanced_layer_layout),
                    Some(&advanced_aux_layout),
                ],
                immediate_size: 0,
            });

        let basic_pipeline = build_pipeline(
            device,
            &shader,
            &basic_pipeline_layout,
            "fs_main_basic",
            surface_format,
            &[],
            "gfx::composition.basic_pipeline",
        );
        let advanced_pipeline = build_pipeline(
            device,
            &shader,
            &advanced_pipeline_layout,
            "fs_main_advanced",
            surface_format,
            &features.as_constants(),
            "gfx::composition.advanced_pipeline",
        );

        Self {
            surface_format,
            features,
            shader,
            frame_layout,
            frame_bind_group,
            basic_pipeline,
            basic_pipeline_layout,
            basic_layer_layout,
            basic_opaque_bg,
            basic_translucent_bg,
            advanced_pipeline,
            advanced_pipeline_layout,
            advanced_layer_layout,
            advanced_opaque_bg,
            advanced_translucent_bg,
            advanced_aux_layout,
            advanced_aux_bind_group,
        }
    }

    pub fn rebuild_with_features(
        &mut self,
        device: &wgpu::Device,
        features: CompositionFeatures,
    ) {
        if self.features == features {
            return;
        }
        tracing::info!(
            soft_shadow = features.soft_shadow,
            volumetric_clouds = features.volumetric_clouds,
            ambient_occlusion = features.ambient_occlusion,
            "composition pipeline features changed; rebuilding advanced",
        );
        self.advanced_pipeline = build_pipeline(
            device,
            &self.shader,
            &self.advanced_pipeline_layout,
            "fs_main_advanced",
            self.surface_format,
            &features.as_constants(),
            "gfx::composition.advanced_pipeline",
        );
        self.features = features;
    }

    /// Recreate every per-layer bind group after the G-buffer's
    /// underlying texture views have changed (resize or mode toggle).
    /// Rebuilds basic always; advanced only when the G-buffer is in
    /// advanced shape.
    pub fn rebuild_gbuffer_bind_groups(&mut self, device: &wgpu::Device, gbuffer: &GBuffer) {
        self.basic_opaque_bg =
            build_basic_layer_bg(device, &self.basic_layer_layout, &gbuffer.opaque, "opaque");
        self.basic_translucent_bg = build_basic_layer_bg(
            device,
            &self.basic_layer_layout,
            &gbuffer.translucent,
            "translucent",
        );
        if gbuffer.is_advanced() {
            self.advanced_opaque_bg = Some(build_advanced_layer_bg(
                device,
                &self.advanced_layer_layout,
                &gbuffer.opaque,
                "opaque",
            ));
            self.advanced_translucent_bg = Some(build_advanced_layer_bg(
                device,
                &self.advanced_layer_layout,
                &gbuffer.translucent,
                "translucent",
            ));
        } else {
            self.advanced_opaque_bg = None;
            self.advanced_translucent_bg = None;
        }
    }

    pub fn rebuild_advanced_aux_bind_group(
        &mut self,
        device: &wgpu::Device,
        shadow_map: &ShadowMap,
        atlases: &Atlases,
    ) {
        self.advanced_aux_bind_group = build_advanced_aux_bind_group(
            device,
            &self.advanced_aux_layout,
            shadow_map,
            atlases,
        );
    }

    /// Record the composition pass for the requested mode. Caller
    /// owns the render pass.
    pub fn record<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>, advanced: bool) {
        if advanced {
            let opaque_bg = self.advanced_opaque_bg.as_ref().expect(
                "advanced composition requires advanced G-buffer; caller must call \
                 rebuild_gbuffer_bind_groups after switching modes",
            );
            let translucent_bg = self.advanced_translucent_bg.as_ref().expect(
                "advanced composition requires advanced G-buffer; caller must call \
                 rebuild_gbuffer_bind_groups after switching modes",
            );
            pass.set_pipeline(&self.advanced_pipeline);
            pass.set_bind_group(0, &self.frame_bind_group, &[]);
            pass.set_bind_group(1, opaque_bg, &[]);
            pass.set_bind_group(2, translucent_bg, &[]);
            pass.set_bind_group(3, &self.advanced_aux_bind_group, &[]);
        } else {
            pass.set_pipeline(&self.basic_pipeline);
            pass.set_bind_group(0, &self.frame_bind_group, &[]);
            pass.set_bind_group(1, &self.basic_opaque_bg, &[]);
            pass.set_bind_group(2, &self.basic_translucent_bg, &[]);
        }
        pass.draw(0..6, 0..1);
    }

    #[must_use]
    pub fn frame_bind_group_layout(&self) -> &wgpu::BindGroupLayout {
        &self.frame_layout
    }

    #[must_use]
    pub fn advanced_aux_bind_group_layout(&self) -> &wgpu::BindGroupLayout {
        &self.advanced_aux_layout
    }
}

fn build_basic_layer_layout(device: &wgpu::Device) -> wgpu::BindGroupLayout {
    // Sparse binding numbers to match the WGSL declarations:
    // diffuse @ binding 0, depth @ binding 3. Bindings 1 (normal) and
    // 2 (material) are absent in basic mode; the shader entries don't
    // reference them.
    device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
        label: Some("gfx::composition.basic_layer_layout"),
        entries: &[
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

fn build_advanced_layer_layout(device: &wgpu::Device) -> wgpu::BindGroupLayout {
    device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
        label: Some("gfx::composition.advanced_layer_layout"),
        entries: &[
            // diffuse — Rgba16Float, sampled as float (non-filterable).
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
            // normal — Rg8Unorm, sampled as float.
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
            // material — R16Uint, sampled as uint.
            wgpu::BindGroupLayoutEntry {
                binding: 2,
                visibility: wgpu::ShaderStages::FRAGMENT,
                ty: wgpu::BindingType::Texture {
                    sample_type: wgpu::TextureSampleType::Uint,
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

fn build_basic_layer_bg(
    device: &wgpu::Device,
    layout: &wgpu::BindGroupLayout,
    layer: &crate::render::gbuffer::GBufferLayer,
    label: &str,
) -> wgpu::BindGroup {
    device.create_bind_group(&wgpu::BindGroupDescriptor {
        label: Some(&format!("gfx::composition.basic_{label}_bg")),
        layout,
        entries: &[
            wgpu::BindGroupEntry {
                binding: 0,
                resource: wgpu::BindingResource::TextureView(&layer.diffuse.view),
            },
            wgpu::BindGroupEntry {
                binding: 3,
                resource: wgpu::BindingResource::TextureView(layer.depth_view()),
            },
        ],
    })
}

fn build_advanced_layer_bg(
    device: &wgpu::Device,
    layout: &wgpu::BindGroupLayout,
    layer: &crate::render::gbuffer::GBufferLayer,
    label: &str,
) -> wgpu::BindGroup {
    let normal = layer
        .normal
        .as_ref()
        .expect("advanced gbuffer layer must have normal");
    let material = layer
        .material
        .as_ref()
        .expect("advanced gbuffer layer must have material");
    device.create_bind_group(&wgpu::BindGroupDescriptor {
        label: Some(&format!("gfx::composition.advanced_{label}_bg")),
        layout,
        entries: &[
            wgpu::BindGroupEntry {
                binding: 0,
                resource: wgpu::BindingResource::TextureView(&layer.diffuse.view),
            },
            wgpu::BindGroupEntry {
                binding: 1,
                resource: wgpu::BindingResource::TextureView(&normal.view),
            },
            wgpu::BindGroupEntry {
                binding: 2,
                resource: wgpu::BindingResource::TextureView(&material.view),
            },
            wgpu::BindGroupEntry {
                binding: 3,
                resource: wgpu::BindingResource::TextureView(layer.depth_view()),
            },
        ],
    })
}

fn build_advanced_aux_layout(device: &wgpu::Device) -> wgpu::BindGroupLayout {
    device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
        label: Some("gfx::composition.advanced_aux_layout"),
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
            wgpu::BindGroupLayoutEntry {
                binding: 3,
                visibility: wgpu::ShaderStages::FRAGMENT,
                ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                count: None,
            },
        ],
    })
}

fn build_advanced_aux_bind_group(
    device: &wgpu::Device,
    layout: &wgpu::BindGroupLayout,
    shadow_map: &ShadowMap,
    atlases: &Atlases,
) -> wgpu::BindGroup {
    device.create_bind_group(&wgpu::BindGroupDescriptor {
        label: Some("gfx::composition.advanced_aux_bg"),
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
                resource: wgpu::BindingResource::Sampler(atlases.noise_sampler()),
            },
        ],
    })
}

fn build_pipeline(
    device: &wgpu::Device,
    shader: &wgpu::ShaderModule,
    pipeline_layout: &wgpu::PipelineLayout,
    fs_entry: &str,
    surface_format: wgpu::TextureFormat,
    constants: &[(&str, f64)],
    label: &str,
) -> wgpu::RenderPipeline {
    let compilation_options = wgpu::PipelineCompilationOptions {
        constants,
        zero_initialize_workgroup_memory: false,
    };
    device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
        label: Some(label),
        layout: Some(pipeline_layout),
        vertex: wgpu::VertexState {
            module: shader,
            entry_point: Some("vs_main"),
            compilation_options: compilation_options.clone(),
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
        depth_stencil: None,
        multisample: wgpu::MultisampleState {
            count: 1,
            mask: !0,
            alpha_to_coverage_enabled: false,
        },
        fragment: Some(wgpu::FragmentState {
            module: shader,
            entry_point: Some(fs_entry),
            compilation_options,
            targets: &[Some(wgpu::ColorTargetState {
                format: surface_format,
                blend: Some(wgpu::BlendState::REPLACE),
                write_mask: wgpu::ColorWrites::ALL,
            })],
        }),
        multiview_mask: None,
        cache: None,
    })
}

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

/// Boolean feature flags that the composition shader picks up via WGSL
/// `override` constants at pipeline-creation time. naga folds the values
/// and dead-code-strips disabled branches, so a turned-off feature has
/// ~zero runtime cost (same semantics as the C++ build's `#define`-gated
/// shader variants in `rendering.ixx::init_pipeline`).
///
/// Toggling any field requires rebuilding the pipeline — see
/// [`CompositionPipeline::rebuild_with_features`].
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CompositionFeatures {
    /// `SOFT_SHADOW`. When on, the shadow lookup uses the full-precision
    /// world-space coord (smooth PCF transition between texels). When
    /// off, the coord is snapped to a 32-unit grid so neighbouring
    /// fragments share shadow texels — eliminates per-fragment shadow
    /// noise at the cost of crisper shadow edges.
    pub soft_shadow: bool,
    /// `VOLUMETRIC_CLOUDS`. Enables the 32-iteration cloud raymarch. Off
    /// by default — heavy.
    pub volumetric_clouds: bool,
    /// `AMBIENT_OCCLUSION`. Enables 16-sample screen-space SSAO. Off by
    /// default — heavy.
    pub ambient_occlusion: bool,
}

impl CompositionFeatures {
    /// Convert to the `&[(&str, f64)]` shape `wgpu::PipelineCompilationOptions`
    /// expects for `override` constants. WGSL `bool` overrides accept any
    /// non-zero `f64` as `true`.
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

/// Deferred composition pipeline + the three bind groups that feed it.
///
/// The G-buffer + shadow bind groups are recreated on resize (see
/// [`Self::rebuild_gbuffer_bind_group`]); the frame and aux bind groups
/// stay constant for the pipeline's lifetime.
pub struct CompositionPipeline {
    pipeline: wgpu::RenderPipeline,
    /// Pipeline layout — kept around so `rebuild_with_features` can stamp
    /// out a fresh `RenderPipeline` against the same group bindings.
    pipeline_layout: wgpu::PipelineLayout,
    /// Compiled shader module. WGSL source compiles once at startup;
    /// rebuilds only swap the `override` constants without re-parsing.
    shader: wgpu::ShaderModule,
    /// Format of the surface the composition pass writes — needed by
    /// rebuilds since `wgpu::ColorTargetState` carries the format.
    surface_format: wgpu::TextureFormat,
    /// Currently-applied feature flags. Used by
    /// [`Self::rebuild_with_features`] to short-circuit when nothing
    /// changed.
    features: CompositionFeatures,
    /// Group 0 — frame uniforms.
    frame_layout: wgpu::BindGroupLayout,
    frame_bind_group: wgpu::BindGroup,
    /// Group 1 — G-buffer attachments. Recreated on resize since the
    /// underlying texture views are recreated then.
    gbuffer_layout: wgpu::BindGroupLayout,
    gbuffer_bind_group: wgpu::BindGroup,
    /// Group 2 — shadow map + noise. Recreated when the shadow map is
    /// resized; the noise atlas is fixed.
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
        features: CompositionFeatures,
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

        let pipeline = build_pipeline(device, &shader, &pipeline_layout, surface_format, features);

        Self {
            pipeline,
            pipeline_layout,
            shader,
            surface_format,
            features,
            frame_layout,
            frame_bind_group,
            gbuffer_layout,
            gbuffer_bind_group,
            aux_layout,
            aux_bind_group,
        }
    }

    /// Rebuild the render pipeline against new feature flags. Cheap when
    /// `features == self.features` (single struct compare, no GPU work).
    /// Otherwise stamps out a fresh `RenderPipeline` — the WGSL source is
    /// already parsed; only the `override` constants change.
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
            "composition pipeline features changed; rebuilding",
        );
        self.pipeline = build_pipeline(
            device,
            &self.shader,
            &self.pipeline_layout,
            self.surface_format,
            features,
        );
        self.features = features;
    }

    /// Recreate the G-buffer bind group after the underlying texture views
    /// have been recreated by [`GBuffer::resize`]. Call from `Game::resize`.
    pub fn rebuild_gbuffer_bind_group(&mut self, device: &wgpu::Device, gbuffer: &GBuffer) {
        self.gbuffer_bind_group =
            Self::build_gbuffer_bind_group(device, &self.gbuffer_layout, gbuffer);
    }

    /// Recreate the aux bind group after the shadow map has been resized
    /// (its `view` is replaced). Noise atlas is stable so we rebind it
    /// unchanged. Called by `Game::apply_shadow_config` whenever
    /// `Config::shadow_res` changes.
    pub fn rebuild_aux_bind_group(
        &mut self,
        device: &wgpu::Device,
        shadow_map: &ShadowMap,
        atlases: &Atlases,
    ) {
        self.aux_bind_group =
            Self::build_aux_bind_group(device, &self.aux_layout, shadow_map, atlases);
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

/// Stamp out the composition `RenderPipeline` against `features`. Shared
/// between [`CompositionPipeline::new`] and
/// [`CompositionPipeline::rebuild_with_features`] so the pipeline state
/// (primitive / blend / multisample / depth-stencil) lives in exactly one
/// place. WGSL `override` constants travel via
/// `PipelineCompilationOptions::constants` — naga folds them and
/// dead-code-strips the disabled feature branches in the shader.
fn build_pipeline(
    device: &wgpu::Device,
    shader: &wgpu::ShaderModule,
    pipeline_layout: &wgpu::PipelineLayout,
    surface_format: wgpu::TextureFormat,
    features: CompositionFeatures,
) -> wgpu::RenderPipeline {
    let constants = features.as_constants();
    let compilation_options = wgpu::PipelineCompilationOptions {
        constants: &constants,
        zero_initialize_workgroup_memory: false,
    };
    device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
        label: Some("gfx::composition.pipeline"),
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
        // No depth attachment — composition writes the surface directly.
        depth_stencil: None,
        multisample: wgpu::MultisampleState {
            count: 1,
            mask: !0,
            alpha_to_coverage_enabled: false,
        },
        fragment: Some(wgpu::FragmentState {
            module: shader,
            entry_point: Some("fs_main"),
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

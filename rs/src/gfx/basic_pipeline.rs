//! A scaffold render pipeline that draws a single colored triangle from
//! three hardcoded vertex positions.
//!
//! `BasicPipeline` is a passive object: it owns only the
//! [`wgpu::RenderPipeline`] handle and exposes a [`BasicPipeline::draw`]
//! method that records `set_pipeline` + `draw(0..3, 0..1)` into a
//! caller-supplied [`wgpu::RenderPass`]. Surface acquisition, the render
//! pass itself, and any depth attachment are the App's responsibility.
//!
//! This pipeline is the integration target real chunk / UI / post
//! pipelines will plug into when `[D1]` / `[D2]` / `[D4]` land — it
//! exists to prove the wgpu wiring before any of the C++ shaders are
//! ported to WGSL.

use wgpu::{
    BlendState, ColorTargetState, ColorWrites, Device, FragmentState, FrontFace,
    MultisampleState, PipelineCompilationOptions, PipelineLayoutDescriptor, PolygonMode,
    PrimitiveState, PrimitiveTopology, RenderPass, RenderPipeline, RenderPipelineDescriptor,
    ShaderModuleDescriptor, ShaderSource, TextureFormat, VertexState,
};

/// Inline WGSL source for the scaffold triangle shader. See
/// `basic.wgsl` next to this file for the actual shader body.
const SHADER_SRC: &str = include_str!("basic.wgsl");

/// Owns the GPU pipeline state object for the scaffold triangle.
///
/// Construct once at startup with [`BasicPipeline::new`]; record draws
/// with [`BasicPipeline::draw`] inside an existing render pass.
#[derive(Debug)]
pub struct BasicPipeline {
    pipeline: RenderPipeline,
}

impl BasicPipeline {
    /// Compile the scaffold pipeline against `color_format`.
    ///
    /// `color_format` should match the format of the render target the
    /// caller will attach when issuing a render pass — typically the
    /// surface's configured format.
    #[must_use]
    pub fn new(device: &Device, color_format: TextureFormat) -> Self {
        let shader = device.create_shader_module(ShaderModuleDescriptor {
            label: Some("gfx::basic_pipeline shader"),
            source: ShaderSource::Wgsl(SHADER_SRC.into()),
        });

        let layout = device.create_pipeline_layout(&PipelineLayoutDescriptor {
            label: Some("gfx::basic_pipeline layout"),
            bind_group_layouts: &[],
            immediate_size: 0,
        });

        let pipeline = device.create_render_pipeline(&RenderPipelineDescriptor {
            label: Some("gfx::basic_pipeline"),
            layout: Some(&layout),
            vertex: VertexState {
                module: &shader,
                entry_point: Some("vs_main"),
                compilation_options: PipelineCompilationOptions::default(),
                buffers: &[],
            },
            primitive: PrimitiveState {
                topology: PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: FrontFace::Ccw,
                cull_mode: None,
                unclipped_depth: false,
                polygon_mode: PolygonMode::Fill,
                conservative: false,
            },
            depth_stencil: None,
            multisample: MultisampleState {
                count: 1,
                mask: !0,
                alpha_to_coverage_enabled: false,
            },
            fragment: Some(FragmentState {
                module: &shader,
                entry_point: Some("fs_main"),
                compilation_options: PipelineCompilationOptions::default(),
                targets: &[Some(ColorTargetState {
                    format: color_format,
                    blend: Some(BlendState::REPLACE),
                    write_mask: ColorWrites::ALL,
                })],
            }),
            multiview_mask: None,
            cache: None,
        });

        Self { pipeline }
    }

    /// Record the triangle draw into the given render pass.
    ///
    /// The pass must already have a color attachment whose format
    /// matches the `color_format` this pipeline was built with.
    pub fn draw<'a>(&'a self, pass: &mut RenderPass<'a>) {
        pass.set_pipeline(&self.pipeline);
        pass.draw(0..3, 0..1);
    }
}

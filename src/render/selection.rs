//! Block-selection wireframe — a 12-edge line-list cube drawn at the
//! currently-selected world block.
//!
//! Lives as a real 3-D pass (alongside chunks + particles) instead of an
//! egui overlay so the outline is properly occluded by world geometry and
//! sits *behind* every UI element. The cube is enlarged by [`SELECTION_EPS`]
//! on each axis to keep the lines off the block faces — preventing
//! reversed-Z fighting at the corner / edge boundary that bare equality
//! comparisons would surface as flickering speckles.

use bytemuck::{Pod, Zeroable};
use cgmath::Vector3;
use wgpu::util::DeviceExt;

use crate::render::depth::DepthTarget;
use crate::render::uniforms::{FrameUniforms, UniformBuffer};

const SHADER_SRC: &str = include_str!("../../shaders/selection.wgsl");

/// Outward expansion (in world units) along each axis of the unit-cube
/// outline. Mirrors the C++ `EPS = 0.005f` in `draw_block_selection_border`
/// — large enough that the lines clear every face of the block without
/// looking visibly oversized.
const SELECTION_EPS: f32 = 0.005;

/// Number of vertices in the wireframe (12 edges × 2 endpoints).
const VERTEX_COUNT: u32 = 24;

/// One vertex of the wireframe — world-space position only.
#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
struct SelectionVertex {
    position: [f32; 3],
}

/// 12 edges of a unit cube, encoded as pairs of `(corner_a, corner_b)`. Each
/// corner is a 3-bit index — bit 0 = X, bit 1 = Y, bit 2 = Z, value 0 / 1
/// indicating min / max corner of that axis.
const EDGES: [(u8, u8); 12] = [
    // bottom (y = 0)
    (0b000, 0b001),
    (0b001, 0b101),
    (0b101, 0b100),
    (0b100, 0b000),
    // top (y = 1)
    (0b010, 0b011),
    (0b011, 0b111),
    (0b111, 0b110),
    (0b110, 0b010),
    // verticals
    (0b000, 0b010),
    (0b001, 0b011),
    (0b101, 0b111),
    (0b100, 0b110),
];

/// Pipeline + GPU resources for the selection wireframe.
pub struct SelectionPipeline {
    pipeline: wgpu::RenderPipeline,
    frame_bg: wgpu::BindGroup,
    /// One static vertex buffer holding the 24-vertex line list. Written by
    /// [`Self::set_block`] each time the selection changes; not freed
    /// between frames so consecutive draws of the same block are zero-cost.
    vertex_buffer: wgpu::Buffer,
    /// `true` once `set_block` has populated the buffer; `false` resets
    /// rendering to a no-op (used when nothing is selected).
    visible: bool,
}

impl SelectionPipeline {
    /// Compile the pipeline. Reads the same `FrameUniforms` group the chunk
    /// pipeline uses, so a single per-frame upload feeds both.
    #[must_use]
    pub fn new(
        device: &wgpu::Device,
        color_format: wgpu::TextureFormat,
        depth_format: wgpu::TextureFormat,
        frame_uniforms: &UniformBuffer<FrameUniforms>,
    ) -> Self {
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("selection_pipeline shader"),
            source: wgpu::ShaderSource::Wgsl(SHADER_SRC.into()),
        });

        let frame_bgl = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("selection_pipeline frame_bgl"),
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

        let frame_bg = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("selection_pipeline frame_bg"),
            layout: &frame_bgl,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: frame_uniforms.binding(),
            }],
        });

        let pipeline_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("selection_pipeline layout"),
            bind_group_layouts: &[Some(&frame_bgl)],
            immediate_size: 0,
        });

        let vertex_attributes = [wgpu::VertexAttribute {
            format: wgpu::VertexFormat::Float32x3,
            offset: 0,
            shader_location: 0,
        }];
        let vertex_layout = wgpu::VertexBufferLayout {
            array_stride: std::mem::size_of::<SelectionVertex>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &vertex_attributes,
        };

        let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("selection_pipeline"),
            layout: Some(&pipeline_layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: Some("vs_main"),
                compilation_options: wgpu::PipelineCompilationOptions::default(),
                buffers: &[vertex_layout],
            },
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::LineList,
                strip_index_format: None,
                front_face: wgpu::FrontFace::Ccw,
                cull_mode: None,
                unclipped_depth: false,
                polygon_mode: wgpu::PolygonMode::Fill,
                conservative: false,
            },
            // Reversed-Z: lines must beat the chunk depth value with a
            // strictly *greater* z. The `SELECTION_EPS` outward push gives
            // them that margin at every face. Depth write stays off so the
            // wireframe doesn't pollute the buffer for later passes
            // (particles, etc.).
            depth_stencil: Some(wgpu::DepthStencilState {
                format: depth_format,
                depth_write_enabled: Some(false),
                depth_compare: Some(wgpu::CompareFunction::Greater),
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
                    // Color inversion: with the shader emitting pure white
                    // (1, 1, 1), `out = src * (1 - dst) + dst * 0 = 1 - dst`.
                    // Wireframe pixels show the inverse of whatever the
                    // world pass drew underneath — always high-contrast,
                    // and visually distinct from any block art.
                    blend: Some(wgpu::BlendState {
                        color: wgpu::BlendComponent {
                            src_factor: wgpu::BlendFactor::OneMinusDst,
                            dst_factor: wgpu::BlendFactor::Zero,
                            operation: wgpu::BlendOperation::Add,
                        },
                        alpha: wgpu::BlendComponent::REPLACE,
                    }),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            multiview_mask: None,
            cache: None,
        });

        // Pre-size the vertex buffer to fit the full 24-vertex line list.
        // We never grow it; the data is rewritten in place on every
        // selection change via `queue.write_buffer`.
        let blank = vec![SelectionVertex { position: [0.0; 3] }; VERTEX_COUNT as usize];
        let vertex_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("selection_pipeline vertex_buffer"),
            contents: bytemuck::cast_slice(&blank),
            usage: wgpu::BufferUsages::VERTEX | wgpu::BufferUsages::COPY_DST,
        });

        Self {
            pipeline,
            frame_bg,
            vertex_buffer,
            visible: false,
        }
    }

    /// Hide the wireframe. Subsequent [`Self::draw`] calls become no-ops
    /// until [`Self::set_block`] is called again.
    pub fn clear(&mut self) {
        self.visible = false;
    }

    /// Refresh the vertex buffer for a new selected block at integer-coord
    /// `coord`. Emits 24 line-list vertices around the unit cube
    /// `[coord, coord + 1]`, expanded outward by [`SELECTION_EPS`] so the
    /// reversed-Z `Greater` test passes against the block's own faces.
    pub fn set_block(&mut self, queue: &wgpu::Queue, coord: Vector3<i32>) {
        let base = [coord.x as f32, coord.y as f32, coord.z as f32];
        // 8 corners of the (slightly enlarged) cube.
        let corners: [[f32; 3]; 8] = std::array::from_fn(|idx| {
            let dx = ((idx & 1) != 0) as i32 as f32;
            let dy = ((idx & 2) != 0) as i32 as f32;
            let dz = ((idx & 4) != 0) as i32 as f32;
            // Push each corner outward along its axis: 0 → -EPS, 1 → 1+EPS.
            let push = |coord_axis: f32, base_axis: f32| -> f32 {
                if coord_axis < 0.5 {
                    base_axis - SELECTION_EPS
                } else {
                    base_axis + 1.0 + SELECTION_EPS
                }
            };
            [
                push(dx, base[0]),
                push(dy, base[1]),
                push(dz, base[2]),
            ]
        });

        let mut verts = [SelectionVertex { position: [0.0; 3] }; VERTEX_COUNT as usize];
        for (i, (a, b)) in EDGES.iter().enumerate() {
            verts[i * 2] = SelectionVertex {
                position: corners[*a as usize],
            };
            verts[i * 2 + 1] = SelectionVertex {
                position: corners[*b as usize],
            };
        }
        queue.write_buffer(&self.vertex_buffer, 0, bytemuck::cast_slice(&verts));
        self.visible = true;
    }

    /// Bind state and draw the 24-vertex line list into `pass`. The pass
    /// must already have a depth attachment matching [`DepthTarget::FORMAT`]
    /// (the format passed to [`Self::new`]) — the chunk render pass does.
    /// No-op when the wireframe is cleared (`set_block` not yet called).
    pub fn draw<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>) {
        if !self.visible {
            return;
        }
        pass.set_pipeline(&self.pipeline);
        pass.set_bind_group(0, &self.frame_bg, &[]);
        pass.set_vertex_buffer(0, self.vertex_buffer.slice(..));
        pass.draw(0..VERTEX_COUNT, 0..1);
    }

    /// Format the pipeline expects on the depth attachment. Provided so the
    /// caller (the render-pass setup in `Game::record_world_pass`) can be
    /// statically asserted against [`DepthTarget::FORMAT`].
    #[must_use]
    pub const fn depth_format() -> wgpu::TextureFormat {
        DepthTarget::FORMAT
    }
}

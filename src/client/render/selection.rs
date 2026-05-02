//! Line-overlay pipeline — draws both the block-selection wireframe
//! (12-edge cube, world-space, occluded by world geometry) and the
//! center-screen crosshair (`+`, screen-space, always on top) through
//! a single `wgpu::RenderPipeline`.
//!
//! Originally two pipelines (`SelectionPipeline` and `CrosshairPipeline`)
//! that differed only in vertex layout, depth-compare op, and projection
//! math. They were merged once it became clear:
//!
//! * Both target the same color attachment with the same `OneMinusDst`
//!   color-inversion blend.
//! * Both render lists of line segments, depth-write off, into the same
//!   depth attachment.
//! * The crosshair's `CompareFunction::Always` is equivalent to `Greater`
//!   when its vertices write clip-space `z = 1.0` — no value the depth
//!   buffer ever holds (cleared to `0.0`, geometry writes `[0, 1)`) can
//!   beat it. So the unified pipeline uses `Greater` and the crosshair
//!   wins on the back of its near-plane z, matching the selection
//!   wireframe's compare op exactly.
//!
//! Each logical line segment is rasterised as a 6-vertex screen-space
//! quad (two triangles). wgpu offers no portable way to widen
//! `LineList` strokes beyond one device pixel; the VS expands each
//! quad-corner perpendicular to the projected segment direction by
//! [`HALF_WIDTH_PX`] pixels. The vertex format gains a `corner` field
//! identifying which of the four quad corners this vertex is and a
//! `kind` discriminator (0 = world, 1 = screen-pixel-offset) that the
//! VS branches on.
//!
//! Buffer layout: 12 crosshair vertices at offsets `0..12` (2 segments
//! × 6 verts), then 72 cube vertices at offsets `12..84` (12 segments
//! × 6 verts). Crosshair vertices are static — set once at construction.
//! Cube vertices are rewritten each time the selection moves via
//! [`Self::set_block`]. The pipeline issues two draws per frame
//! (always 0..12 for the crosshair, optionally 12..84 for the cube
//! when a block is selected).

use bytemuck::{Pod, Zeroable};
use cgmath::Vector3;
use wgpu::util::DeviceExt;

use crate::client::render::depth::DepthTarget;
use crate::client::render::uniforms::{FrameUniforms, UniformBuffer};

const SHADER_SRC: &str = include_str!("../../../shaders/selection.wgsl");

/// Half-length of each crosshair arm in screen pixels.
const CROSSHAIR_HALF_LEN_PX: f32 = 12.0;

/// Vertices per logical line segment — six because each segment is two
/// triangles in `TriangleList` order. Per-segment vertex sequence:
/// `[A+, A-, B+, B+, A-, B-]` where `±` is the perpendicular extrude
/// side (see `CornerId` mapping in the WGSL shader).
const VERTS_PER_SEGMENT: u32 = 6;

/// 2 crosshair segments × 6 verts each, parked at the front of the buffer.
const CROSSHAIR_VERTEX_BASE: u32 = 0;
const CROSSHAIR_SEGMENT_COUNT: u32 = 2;
const CROSSHAIR_VERTEX_COUNT: u32 = CROSSHAIR_SEGMENT_COUNT * VERTS_PER_SEGMENT;
/// 12 cube edges × 6 verts each, immediately after the crosshair.
const SELECTION_VERTEX_BASE: u32 = CROSSHAIR_VERTEX_COUNT;
const SELECTION_SEGMENT_COUNT: u32 = 12;
const SELECTION_VERTEX_COUNT: u32 = SELECTION_SEGMENT_COUNT * VERTS_PER_SEGMENT;
const TOTAL_VERTEX_COUNT: u32 = CROSSHAIR_VERTEX_COUNT + SELECTION_VERTEX_COUNT;

/// Vertex kind discriminator — must match the WGSL `KIND_*` constants.
const KIND_WORLD: u32 = 0;
const KIND_SCREEN: u32 = 1;

/// Quad-corner ids — must match the WGSL `CORNER_*` constants.
const CORNER_A_PLUS: u32 = 0;
const CORNER_A_MINUS: u32 = 1;
const CORNER_B_PLUS: u32 = 2;
const CORNER_B_MINUS: u32 = 3;

/// Triangle-list winding for one segment's quad: `(A+, A-, B+)` then
/// `(B+, A-, B-)`. Both triangles are CCW under `front_face: Ccw` (cull
/// is disabled anyway, so winding is informational).
const SEGMENT_CORNERS: [u32; VERTS_PER_SEGMENT as usize] = [
    CORNER_A_PLUS,
    CORNER_A_MINUS,
    CORNER_B_PLUS,
    CORNER_B_PLUS,
    CORNER_A_MINUS,
    CORNER_B_MINUS,
];

/// One vertex of the line overlay. `a` and `b` are the segment's two
/// endpoints in either world coords (`KIND_WORLD`) or screen pixel
/// offsets (`KIND_SCREEN`, `z` ignored). `corner` selects this
/// vertex's role in the quad; the VS uses it to pick which endpoint
/// to project and which perpendicular side to extrude toward.
#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
struct OverlayVertex {
    a: [f32; 3],
    b: [f32; 3],
    corner: u32,
    kind: u32,
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

/// Pipeline + GPU resources for the line-overlay pass.
pub struct SelectionPipeline {
    pipeline: wgpu::RenderPipeline,
    frame_bg: wgpu::BindGroup,
    /// Single buffer holding both the static crosshair and the dynamic
    /// selection cube. See module-level layout comment.
    vertex_buffer: wgpu::Buffer,
    /// `true` once `set_block` has populated the cube portion; `false`
    /// resets cube rendering to a no-op (used when nothing is selected).
    /// The crosshair always draws regardless.
    cube_visible: bool,
}

impl SelectionPipeline {
    /// Compile the pipeline. Reads the same `FrameUniforms` group the chunk
    /// pipeline uses, so a single per-frame upload feeds both.
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

        let vertex_attributes = [
            // a
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Float32x3,
                offset: 0,
                shader_location: 0,
            },
            // b
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Float32x3,
                offset: 12,
                shader_location: 1,
            },
            // corner
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Uint32,
                offset: 24,
                shader_location: 2,
            },
            // kind
            wgpu::VertexAttribute {
                format: wgpu::VertexFormat::Uint32,
                offset: 28,
                shader_location: 3,
            },
        ];
        let vertex_layout = wgpu::VertexBufferLayout {
            array_stride: std::mem::size_of::<OverlayVertex>() as wgpu::BufferAddress,
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
                // Triangles, not lines — we expand each segment into a
                // quad in the VS so we can have a configurable pixel
                // thickness. `LineList` would always rasterise at one
                // device pixel.
                topology: wgpu::PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: wgpu::FrontFace::Ccw,
                cull_mode: None,
                unclipped_depth: false,
                polygon_mode: wgpu::PolygonMode::Fill,
                conservative: false,
            },
            // The crosshair wins because its verts emit clip-space `z = 1.0`
            // (reversed-Z near plane); no value in the depth buffer can
            // exceed that. Depth-write off so neither overlay perturbs
            // the buffer for later passes (particles, etc.).
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
                    // Pixels show the inverse of whatever was already there.
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

        // Pre-populate the buffer with the static crosshair quads in
        // slots 0..CROSSHAIR_VERTEX_COUNT, leaving the cube portion zero
        // until the first `set_block` call.
        let mut verts = [OverlayVertex {
            a: [0.0; 3],
            b: [0.0; 3],
            corner: CORNER_A_PLUS,
            kind: KIND_WORLD,
        }; TOTAL_VERTEX_COUNT as usize];
        let l = CROSSHAIR_HALF_LEN_PX;
        // Horizontal arm: A = (-l, 0), B = (l, 0).
        write_segment(&mut verts, 0, [-l, 0.0, 0.0], [l, 0.0, 0.0], KIND_SCREEN);
        // Vertical arm: A = (0, -l), B = (0, l).
        write_segment(&mut verts, 1, [0.0, -l, 0.0], [0.0, l, 0.0], KIND_SCREEN);
        let vertex_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("selection_pipeline vertex_buffer"),
            contents: bytemuck::cast_slice(&verts),
            usage: wgpu::BufferUsages::VERTEX | wgpu::BufferUsages::COPY_DST,
        });

        Self {
            pipeline,
            frame_bg,
            vertex_buffer,
            cube_visible: false,
        }
    }

    /// Hide the selection cube. The crosshair is unaffected — it always
    /// draws. Subsequent [`Self::draw_cube`] calls become no-ops until
    /// [`Self::set_block`] is called again.
    pub fn clear(&mut self) {
        self.cube_visible = false;
    }

    /// Refresh the cube portion of the vertex buffer for a new selected
    /// block at integer-coord `coord`. Emits 12 segments × 6 verts each
    /// around the unit cube `[coord, coord + 1]`, expanded outward by
    /// [`SELECTION_EPS`] so the reversed-Z `Greater` test passes against
    /// the block's own faces.
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
                    base_axis
                } else {
                    base_axis + 1.0
                }
            };
            [push(dx, base[0]), push(dy, base[1]), push(dz, base[2])]
        });

        let mut verts = [OverlayVertex {
            a: [0.0; 3],
            b: [0.0; 3],
            corner: CORNER_A_PLUS,
            kind: KIND_WORLD,
        }; SELECTION_VERTEX_COUNT as usize];
        for (i, (a_idx, b_idx)) in EDGES.iter().enumerate() {
            write_segment(
                &mut verts,
                i,
                corners[*a_idx as usize],
                corners[*b_idx as usize],
                KIND_WORLD,
            );
        }
        let offset_bytes =
            (SELECTION_VERTEX_BASE as u64) * std::mem::size_of::<OverlayVertex>() as u64;
        queue.write_buffer(
            &self.vertex_buffer,
            offset_bytes,
            bytemuck::cast_slice(&verts),
        );
        self.cube_visible = true;
    }

    /// Bind state and draw the selection cube only. Skipped when no
    /// block is selected. Called *before* the underwater pass in
    /// [`crate::game::Game::record_world_pass`] so terrain depth occludes
    /// the wireframe correctly and the water tint can layer over it.
    pub fn draw_cube<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>) {
        if !self.cube_visible {
            return;
        }
        self.bind(pass);
        pass.draw(
            SELECTION_VERTEX_BASE..(SELECTION_VERTEX_BASE + SELECTION_VERTEX_COUNT),
            0..1,
        );
    }

    /// Bind state and draw the crosshair only. Always issues the draw.
    /// Called *after* the underwater pass so the `+` stays readable
    /// through the water-tint overlay — same layering the standalone
    /// crosshair pipeline had before the merge.
    pub fn draw_crosshair<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>) {
        self.bind(pass);
        pass.draw(
            CROSSHAIR_VERTEX_BASE..(CROSSHAIR_VERTEX_BASE + CROSSHAIR_VERTEX_COUNT),
            0..1,
        );
    }

    fn bind<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>) {
        pass.set_pipeline(&self.pipeline);
        pass.set_bind_group(0, &self.frame_bg, &[]);
        pass.set_vertex_buffer(0, self.vertex_buffer.slice(..));
    }

    /// Format the pipeline expects on the depth attachment. Provided so the
    /// caller (the render-pass setup in `Game::record_world_pass`) can be
    /// statically asserted against [`DepthTarget::FORMAT`].
    pub const fn depth_format() -> wgpu::TextureFormat {
        DepthTarget::FORMAT
    }
}

/// Fill the 6 vertices of segment `seg_idx` in `verts`. `seg_idx`
/// is local to whichever sub-range the caller is writing
/// (crosshair-relative or cube-relative).
fn write_segment(verts: &mut [OverlayVertex], seg_idx: usize, a: [f32; 3], b: [f32; 3], kind: u32) {
    let base = seg_idx * VERTS_PER_SEGMENT as usize;
    for (i, &corner) in SEGMENT_CORNERS.iter().enumerate() {
        verts[base + i] = OverlayVertex { a, b, corner, kind };
    }
}

//! GPU pipeline for particle billboards.
//!
//! Particles render as small camera-facing quads. Vertex format is its own
//! (decoupled from chunk meshing), and the pipeline samples
//! `Atlases::block_diffuse` so particles inherit block textures.
//!
//! See `docs/rust_migration.md` §4.13 / §5 [D3].
//!
//! Each [`Particle`] expands into 6 vertices (two triangles forming a quad).
//! The vertex shader nudges each corner along the camera-space right/up
//! basis vectors, derived from the per-frame view matrix in
//! `FrameUniforms` — so quads always face the camera regardless of where
//! the particle is in the world.

use bytemuck::{Pod, Zeroable};
use wgpu::util::DeviceExt;

use crate::render::{FrameUniforms, UniformBuffer};
use crate::textures::Atlases;
use crate::particles::Particle;

// ----------------------------------------------------------------------
//   Vertex layout
// ----------------------------------------------------------------------

/// Per-vertex data uploaded to the GPU.
///
/// Layout (32 bytes, alignment 4):
/// * `world_pos` — particle center, world space (12 B).
/// * `size` — half-extent of the quad in world units (4 B).
/// * `uv` — texture-face UV in `0..=1` (8 B).
/// * `layer` — array layer index into the block diffuse atlas (4 B).
/// * `_pad` — explicit tail padding so the field count is even and the
///   total size is a multiple of 16 (matches the WGSL storage-buffer
///   alignment expectations).
#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct ParticleVertex {
    pub world_pos: [f32; 3],
    pub size: f32,
    pub uv: [f32; 2],
    pub layer: u32,
    /// Tail padding so the struct size is a multiple of 16. Always 0 in
    /// practice (Pod-zeroed); not consumed by the shader.
    _pad: u32,
}

impl ParticleVertex {
    /// `wgpu` vertex layout descriptor matching this struct.
    #[must_use]
    pub fn layout() -> wgpu::VertexBufferLayout<'static> {
        const ATTRIBUTES: &[wgpu::VertexAttribute] = &wgpu::vertex_attr_array![
            0 => Float32x3,    // world_pos
            1 => Float32,      // size
            2 => Float32x2,    // uv
            3 => Uint32,       // layer
            // _pad is unused by the shader.
        ];
        wgpu::VertexBufferLayout {
            array_stride: std::mem::size_of::<ParticleVertex>() as wgpu::BufferAddress,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: ATTRIBUTES,
        }
    }
}

#[allow(clippy::manual_is_multiple_of)]
const _: () = assert!(std::mem::size_of::<ParticleVertex>() == 32);
#[allow(clippy::manual_is_multiple_of)]
const _: () = assert!(std::mem::size_of::<ParticleVertex>() % 16 == 0);

// ----------------------------------------------------------------------
//   Mesh
// ----------------------------------------------------------------------

/// CPU-side vertex builder. Produces 6 vertices per [`Particle`] in the
/// triangle layout the shader expects:
///
/// ```text
///   2---3        triangles: (0,1,2) (0,2,3)
///   |  /|        UVs: (0,0) (1,0) (1,1) (1,1) (0,1) (0,0)
///   | / |
///   |/  |
///   0---1
/// ```
fn build_vertices(particles: &[Particle]) -> Vec<ParticleVertex> {
    // Per-corner UVs for the triangle pair (BL, BR, TR, TR, TL, BL).
    const CORNER_UVS: [[f32; 2]; 6] = [
        [0.0, 0.0],
        [1.0, 0.0],
        [1.0, 1.0],
        [1.0, 1.0],
        [0.0, 1.0],
        [0.0, 0.0],
    ];

    let mut out = Vec::with_capacity(particles.len() * 6);
    for p in particles {
        let world_pos = [
            p.coord.x as f32,
            p.coord.y as f32,
            p.coord.z as f32,
        ];
        let size = p.extent as f32;
        let layer = p.texture_layer;
        for uv in CORNER_UVS {
            out.push(ParticleVertex {
                world_pos,
                size,
                uv,
                layer,
                _pad: 0,
            });
        }
    }
    out
}

/// GPU vertex buffer for the current particle set.
pub struct ParticleMesh {
    pub buffer: Option<wgpu::Buffer>,
    pub vertex_count: u32,
}

impl Default for ParticleMesh {
    fn default() -> Self {
        Self::new()
    }
}

impl ParticleMesh {
    #[must_use]
    pub fn new() -> Self {
        Self {
            buffer: None,
            vertex_count: 0,
        }
    }

    /// Replace the GPU buffer with one containing the vertex data for
    /// `particles`. With an empty input this drops the buffer; subsequent
    /// [`ParticleMesh::draw`] calls become no-ops.
    pub fn rebuild(&mut self, device: &wgpu::Device, particles: &[Particle]) {
        if particles.is_empty() {
            self.buffer = None;
            self.vertex_count = 0;
            return;
        }
        let verts = build_vertices(particles);
        let buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("particle_mesh vertices"),
            contents: bytemuck::cast_slice(&verts),
            usage: wgpu::BufferUsages::VERTEX,
        });
        self.buffer = Some(buffer);
        self.vertex_count = verts.len() as u32;
    }

    /// Bind the vertex buffer and issue the draw. No-op if the mesh is empty.
    pub fn draw<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>) {
        let Some(buffer) = self.buffer.as_ref() else {
            return;
        };
        if self.vertex_count == 0 {
            return;
        }
        pass.set_vertex_buffer(0, buffer.slice(..));
        pass.draw(0..self.vertex_count, 0..1);
    }
}

// ----------------------------------------------------------------------
//   Pipeline
// ----------------------------------------------------------------------

const SHADER_SRC: &str = include_str!("../../shaders/particle.wgsl");

/// Render pipeline + bind groups for drawing [`ParticleMesh`].
///
/// Bind groups:
/// * group 0 — `FrameUniforms` (uniform buffer).
/// * group 1 — `block_diffuse` D2-array texture + sampler.
pub struct ParticlePipeline {
    pipeline: wgpu::RenderPipeline,
    frame_bg: wgpu::BindGroup,
    atlas_bg: wgpu::BindGroup,
}

impl ParticlePipeline {
    /// Compile the pipeline against `color_format` / `depth_format`.
    ///
    /// `frame_uniforms` and `atlases` must outlive the bind groups stored
    /// inside this pipeline (the bind groups hold references to their
    /// resources).
    #[must_use]
    pub fn new(
        device: &wgpu::Device,
        color_format: wgpu::TextureFormat,
        depth_format: wgpu::TextureFormat,
        frame_uniforms: &UniformBuffer<FrameUniforms>,
        atlases: &Atlases,
    ) -> Self {
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("gfx::particle_render shader"),
            source: wgpu::ShaderSource::Wgsl(SHADER_SRC.into()),
        });

        let frame_bgl = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("particle_pipeline frame_bgl"),
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

        let atlas_bgl = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("particle_pipeline atlas_bgl"),
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: false },
                        view_dimension: wgpu::TextureViewDimension::D2Array,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::NonFiltering),
                    count: None,
                },
            ],
        });

        let frame_bg = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("particle_pipeline frame_bg"),
            layout: &frame_bgl,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: frame_uniforms.binding(),
            }],
        });

        let atlas_bg = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("particle_pipeline atlas_bg"),
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

        let layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("particle_pipeline layout"),
            bind_group_layouts: &[Some(&frame_bgl), Some(&atlas_bgl)],
            immediate_size: 0,
        });

        let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("particle_pipeline"),
            layout: Some(&layout),
            vertex: wgpu::VertexState {
                module: &shader,
                entry_point: Some("vs_main"),
                compilation_options: wgpu::PipelineCompilationOptions::default(),
                buffers: &[ParticleVertex::layout()],
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
            depth_stencil: Some(wgpu::DepthStencilState {
                format: depth_format,
                depth_write_enabled: Some(true),
                // Reversed-Z (matches the rest of the renderer per the
                // migration plan §6).
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
                    blend: Some(wgpu::BlendState::ALPHA_BLENDING),
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            multiview_mask: None,
            cache: None,
        });

        Self {
            pipeline,
            frame_bg,
            atlas_bg,
        }
    }

    /// Bind state and dispatch the mesh's draw call. The caller supplies an
    /// open render pass with color + depth attachments matching the formats
    /// the pipeline was built with.
    pub fn render<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>, mesh: &'a ParticleMesh) {
        if mesh.buffer.is_none() || mesh.vertex_count == 0 {
            return;
        }
        pass.set_pipeline(&self.pipeline);
        pass.set_bind_group(0, &self.frame_bg, &[]);
        pass.set_bind_group(1, &self.atlas_bg, &[]);
        mesh.draw(pass);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::math::Vec3d;
    use cgmath::Zero;

    #[test]
    fn vertex_size_and_alignment() {
        assert_eq!(std::mem::size_of::<ParticleVertex>(), 32);
        assert_eq!(std::mem::align_of::<ParticleVertex>(), 4);
    }

    #[test]
    fn build_vertices_emits_six_per_particle() {
        let ps = vec![
            Particle::new(Vec3d::new(0.0, 0.0, 0.0), Vec3d::zero(), 3, 1.0),
            Particle::new(Vec3d::new(1.0, 2.0, 3.0), Vec3d::zero(), 7, 1.0),
        ];
        let v = build_vertices(&ps);
        assert_eq!(v.len(), 12);
        assert_eq!(v[0].layer, 3);
        assert_eq!(v[6].layer, 7);
        // f32 arrays — exact equality is fine here because the inputs are
        // all integer-valued constants.
        let approx_eq = |a: [f32; 3], b: [f32; 3]| {
            (a[0] - b[0]).abs() < 1e-6
                && (a[1] - b[1]).abs() < 1e-6
                && (a[2] - b[2]).abs() < 1e-6
        };
        let approx_eq2 = |a: [f32; 2], b: [f32; 2]| {
            (a[0] - b[0]).abs() < 1e-6 && (a[1] - b[1]).abs() < 1e-6
        };
        assert!(approx_eq(v[6].world_pos, [1.0, 2.0, 3.0]));
        // First and last UVs of each particle's quad are (0,0) — closes the
        // triangle pair.
        assert!(approx_eq2(v[0].uv, [0.0, 0.0]));
        assert!(approx_eq2(v[5].uv, [0.0, 0.0]));
    }

    #[test]
    fn empty_mesh_draw_is_a_noop() {
        let mesh = ParticleMesh::new();
        // Without a buffer, calling draw should not panic. We can't
        // construct a real RenderPass without a Device, but we can verify
        // the buffer-state branch directly.
        assert!(mesh.buffer.is_none());
        assert_eq!(mesh.vertex_count, 0);
    }
}

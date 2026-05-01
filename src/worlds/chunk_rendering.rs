//! Chunk render upload + draw dispatch ([D2] in `docs/rust_migration.md` §5).
//!
//! Builds the wgpu side of chunk rendering on top of the meshing CPU output
//! produced by [D1] (`gfx::mesh::MeshOutput`):
//!
//! * [`ChunkMesh`] — per-chunk vertex buffers (opaque + translucent).
//! * [`ChunkPipeline`] — render pipelines (one per layer) plus the bind groups
//!   for frame uniforms (group 0) and the block atlas (group 1).
//!
//! The C++ counterpart is `src/worlds/chunk_rendering.cpp` (mesh upload via
//! `render::VertexArray::create`) and `src/worlds/world_rendering.cpp`
//! (per-chunk model uniform write + draw call). The Rust port differs in two
//! ways:
//!
//! * **Per-chunk world origin is baked into vertex positions at upload time.**
//!   `ChunkMesh::upload` adds `coord * CHUNK_SIZE` to every vertex's
//!   `position`, removing the per-chunk model uniform (which the C++ build
//!   uploads via `model_uniforms.set<".u_translation">`). The shader becomes
//!   one uniform-buffer + one texture-array binding.
//! * **No `TRIANGLE_FAN` topology.** [D1] emits 6 vertices per face (two
//!   triangles); the pipeline uses `TriangleList` and a plain `pass.draw(0..n,
//!   0..1)` with no index buffer.

use wgpu::util::DeviceExt;

use crate::render::gbuffer::GBuffer;
use crate::render::mesh::{CHUNK_SIZE, ChunkVertex, MeshOutput};
use crate::render::uniforms::{FrameUniforms, UniformBuffer};
use crate::textures::Atlases;

const SHADER_SRC: &str = include_str!("../../shaders/chunk.wgsl");

/// Vertex stride in bytes — must match the WGSL header in `chunk.wgsl`.
const VERTEX_STRIDE: wgpu::BufferAddress =
    std::mem::size_of::<ChunkVertex>() as wgpu::BufferAddress;

// Compile-time check that `ChunkVertex` packs to the documented 32
// bytes. If the [D1] meshing struct grows, the shader vertex layout
// must be updated in lockstep.
const _: () = assert!(std::mem::size_of::<ChunkVertex>() == 32);

// ---------- ChunkMesh ----------

/// GPU-side mesh for one chunk: up to two vertex buffers (opaque +
/// translucent) plus the world coord they belong to.
///
/// A buffer is `None` when the corresponding `MeshOutput` vec was empty —
/// the C++ build skips the GL draw in that case via `if (!va) continue;`,
/// and the Rust port mirrors that with `Option<wgpu::Buffer>`.
pub struct ChunkMesh {
    /// Chunk world coord (in chunks, not blocks). The vertex positions stored
    /// in the buffers below already have `coord * CHUNK_SIZE` baked in; this
    /// field is kept for debug / culling.
    pub coord: cgmath::Vector3<i32>,
    /// Opaque-layer vertex buffer (depth-write, no blend).
    pub opaque: Option<wgpu::Buffer>,
    /// Number of vertices in `opaque` (0 if `opaque` is `None`).
    pub opaque_count: u32,
    /// Translucent-layer vertex buffer (alpha-blend, no depth-write).
    pub translucent: Option<wgpu::Buffer>,
    /// Number of vertices in `translucent` (0 if `translucent` is `None`).
    pub translucent_count: u32,
}

impl ChunkMesh {
    /// Upload one mesh to the GPU.
    ///
    /// Bakes the chunk world origin into every vertex's `position` before
    /// uploading: each `position` becomes `position + coord * CHUNK_SIZE`,
    /// removing the need for a per-chunk model uniform. Empty layers
    /// produce `None` buffers.
    #[must_use]
    pub fn upload(device: &wgpu::Device, mesh: &MeshOutput) -> Self {
        let origin = [
            (mesh.coord.x * CHUNK_SIZE as i32) as f32,
            (mesh.coord.y * CHUNK_SIZE as i32) as f32,
            (mesh.coord.z * CHUNK_SIZE as i32) as f32,
        ];

        let opaque_count = u32::try_from(mesh.opaque.len()).unwrap_or(u32::MAX);
        let opaque = if mesh.opaque.is_empty() {
            None
        } else {
            Some(create_vertex_buffer(
                device,
                &mesh.opaque,
                origin,
                "gfx::chunk_render.opaque",
            ))
        };

        let translucent_count = u32::try_from(mesh.translucent.len()).unwrap_or(u32::MAX);
        let translucent = if mesh.translucent.is_empty() {
            None
        } else {
            Some(create_vertex_buffer(
                device,
                &mesh.translucent,
                origin,
                "gfx::chunk_render.translucent",
            ))
        };

        Self {
            coord: mesh.coord,
            opaque,
            opaque_count,
            translucent,
            translucent_count,
        }
    }

    /// Record the opaque draw into `pass`. No-op if there is no opaque buffer.
    ///
    /// The caller is responsible for binding the pipeline and bind groups
    /// first via [`ChunkPipeline::begin_opaque`].
    pub fn draw_opaque<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>) {
        if let Some(buffer) = self.opaque.as_ref()
            && self.opaque_count > 0
        {
            pass.set_vertex_buffer(0, buffer.slice(..));
            pass.draw(0..self.opaque_count, 0..1);
        }
    }

    /// Record the translucent draw into `pass`. No-op if there is no
    /// translucent buffer.
    pub fn draw_translucent<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>) {
        if let Some(buffer) = self.translucent.as_ref()
            && self.translucent_count > 0
        {
            pass.set_vertex_buffer(0, buffer.slice(..));
            pass.draw(0..self.translucent_count, 0..1);
        }
    }
}

/// Translate every vertex in `verts` by `origin` and upload as a `VERTEX |
/// COPY_DST` buffer.
fn create_vertex_buffer(
    device: &wgpu::Device,
    verts: &[ChunkVertex],
    origin: [f32; 3],
    label: &str,
) -> wgpu::Buffer {
    // Pre-translate on the CPU so the GPU-side draw has zero per-chunk state.
    let translated: Vec<ChunkVertex> = verts
        .iter()
        .map(|v| ChunkVertex {
            position: [
                v.position[0] + origin[0],
                v.position[1] + origin[1],
                v.position[2] + origin[2],
            ],
            uv: v.uv,
            layer: v.layer,
            face: v.face,
            light: v.light,
        })
        .collect();
    device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
        label: Some(label),
        contents: bytemuck::cast_slice(&translated),
        usage: wgpu::BufferUsages::VERTEX | wgpu::BufferUsages::COPY_DST,
    })
}

// ---------- ChunkPipeline ----------

/// Four deferred render pipelines — basic and advanced share the same
/// vertex format / bind groups / depth state, only the fragment entry
/// point + color targets differ.
///
/// * `basic_opaque` / `basic_translucent` — single-target writes into
///   the G-buffer's `Rgba16Float` diffuse attachment via
///   `fs_main_basic`. Translucent flips alpha blending on; opaque
///   replaces.
/// * `advanced_opaque` / `advanced_translucent` — MRT writes into the
///   full advanced G-buffer (diffuse + normal + material) via
///   `fs_main` / `fs_main_translucent`.
pub struct ChunkPipeline {
    /// Layout for group 0 (frame uniforms).
    frame_layout: wgpu::BindGroupLayout,
    /// Layout for group 1 (block atlas + sampler).
    atlas_layout: wgpu::BindGroupLayout,
    /// Layout for group 2 (opaque depth) — translucent pipelines only.
    opaque_depth_layout: wgpu::BindGroupLayout,
    frame_bind_group: wgpu::BindGroup,
    atlas_bind_group: wgpu::BindGroup,
    /// Group 2 bind group exposing the opaque-layer depth attachment as
    /// a sampled texture. Translucent fragments load it and `discard`
    /// when behind opaque (the translucent pass owns its own depth
    /// attachment, so we can't depth-test against opaque via the
    /// fixed-function pipeline). Rebuilt on resize / mode toggle —
    /// see [`Self::rebuild_opaque_depth_bind_group`].
    opaque_depth_bind_group: wgpu::BindGroup,
    basic_opaque: wgpu::RenderPipeline,
    basic_translucent: wgpu::RenderPipeline,
    advanced_opaque: wgpu::RenderPipeline,
    advanced_translucent: wgpu::RenderPipeline,
}

impl ChunkPipeline {
    /// Compile all four deferred pipelines and build their bind groups.
    /// All four target the G-buffer (no surface format involved); the
    /// composition pass is what eventually writes the surface.
    #[must_use]
    pub fn new(
        device: &wgpu::Device,
        frame_uniforms: &UniformBuffer<FrameUniforms>,
        atlases: &Atlases,
        gbuffer: &GBuffer,
    ) -> Self {
        let depth_format = GBuffer::DEPTH_FORMAT;
        let shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("gfx::chunk_render.shader"),
            source: wgpu::ShaderSource::Wgsl(SHADER_SRC.into()),
        });

        let frame_layout = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("gfx::chunk_render.frame_layout"),
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
            label: Some("gfx::chunk_render.atlas_layout"),
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
                // Normal atlas — sampled per-fragment to apply
                // tangent-space normal-mapping perturbation. Same
                // sampler as diffuse (filtering, repeat-wrapped) since
                // the per-block art tile and the per-block normal tile
                // share the same UV layout.
                wgpu::BindGroupLayoutEntry {
                    binding: 2,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: true },
                        view_dimension: wgpu::TextureViewDimension::D2Array,
                        multisampled: false,
                    },
                    count: None,
                },
            ],
        });

        let frame_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("gfx::chunk_render.frame_bind_group"),
            layout: &frame_layout,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: frame_uniforms.binding(),
            }],
        });

        let atlas_bind_group = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("gfx::chunk_render.atlas_bind_group"),
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
                wgpu::BindGroupEntry {
                    binding: 2,
                    resource: wgpu::BindingResource::TextureView(&atlases.block_normal.view),
                },
            ],
        });

        // Group 2 (translucent only) — opaque depth as a sampled
        // depth texture. Fragment shader uses it for the
        // "discard if behind opaque" early-out.
        let opaque_depth_layout =
            device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
                label: Some("gfx::chunk_render.opaque_depth_layout"),
                entries: &[wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Depth,
                        view_dimension: wgpu::TextureViewDimension::D2,
                        multisampled: false,
                    },
                    count: None,
                }],
            });
        let opaque_depth_bind_group =
            build_opaque_depth_bind_group(device, &opaque_depth_layout, gbuffer);

        let opaque_pipeline_layout =
            device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
                label: Some("gfx::chunk_render.opaque_pipeline_layout"),
                bind_group_layouts: &[Some(&frame_layout), Some(&atlas_layout)],
                immediate_size: 0,
            });
        let translucent_pipeline_layout =
            device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
                label: Some("gfx::chunk_render.translucent_pipeline_layout"),
                bind_group_layouts: &[
                    Some(&frame_layout),
                    Some(&atlas_layout),
                    Some(&opaque_depth_layout),
                ],
                immediate_size: 0,
            });

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
        ];

        let vertex_buffers = [wgpu::VertexBufferLayout {
            array_stride: VERTEX_STRIDE,
            step_mode: wgpu::VertexStepMode::Vertex,
            attributes: &vertex_attributes,
        }];

        // Opaque keeps back-face culling — never see the inside of solid
        // geometry, so the rasterizer can drop half the fragments.
        let opaque_primitive = wgpu::PrimitiveState {
            topology: wgpu::PrimitiveTopology::TriangleList,
            strip_index_format: None,
            front_face: wgpu::FrontFace::Ccw,
            cull_mode: Some(wgpu::Face::Back),
            unclipped_depth: false,
            polygon_mode: wgpu::PolygonMode::Fill,
            conservative: false,
        };
        // Translucent disables culling so a swimmer looking outward through
        // a water column still sees the back of the surface (and any inner
        // wall of a glass enclosure renders without holes).
        let translucent_primitive = wgpu::PrimitiveState {
            cull_mode: None,
            ..opaque_primitive
        };

        let multisample = wgpu::MultisampleState {
            count: 1,
            mask: !0,
            alpha_to_coverage_enabled: false,
        };

        // Reversed-Z: near = 1, far = 0; pass survives if the new
        // fragment is *greater* than what's already in the depth
        // buffer. Both opaque and translucent depth-write so the
        // composition pass / forward overlays can read the world
        // surface (incl. translucent water) for SSR / occlusion.
        let depth_state = wgpu::DepthStencilState {
            format: depth_format,
            depth_write_enabled: Some(true),
            depth_compare: Some(wgpu::CompareFunction::Greater),
            stencil: wgpu::StencilState::default(),
            bias: wgpu::DepthBiasState::default(),
        };

        // Color targets. Every blend is REPLACE: each layer keeps the
        // front-most fragment (depth-tested) for that surface, and
        // composition manually blends the two layers. Basic mode
        // writes a single Rgba16Float diffuse attachment; advanced
        // adds a Rg8Unorm octahedral normal and an R16Uint atlas-layer
        // id (= "material") so composition can sample materials,
        // normals, and emissive (in diffuse.a) per pixel.
        let basic_targets = [Some(wgpu::ColorTargetState {
            format: GBuffer::DIFFUSE_FORMAT,
            blend: None,
            write_mask: wgpu::ColorWrites::ALL,
        })];
        let advanced_targets = [
            Some(wgpu::ColorTargetState {
                format: GBuffer::DIFFUSE_FORMAT,
                blend: None,
                write_mask: wgpu::ColorWrites::ALL,
            }),
            Some(wgpu::ColorTargetState {
                format: GBuffer::NORMAL_FORMAT,
                blend: None,
                write_mask: wgpu::ColorWrites::ALL,
            }),
            Some(wgpu::ColorTargetState {
                format: GBuffer::MATERIAL_FORMAT,
                blend: None,
                write_mask: wgpu::ColorWrites::ALL,
            }),
        ];

        let make_pipeline = |label: &str,
                             pipeline_layout: &wgpu::PipelineLayout,
                             entry: &str,
                             primitive: wgpu::PrimitiveState,
                             targets: &[Option<wgpu::ColorTargetState>]|
         -> wgpu::RenderPipeline {
            device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
                label: Some(label),
                layout: Some(pipeline_layout),
                vertex: wgpu::VertexState {
                    module: &shader,
                    entry_point: Some("vs_main"),
                    compilation_options: wgpu::PipelineCompilationOptions::default(),
                    buffers: &vertex_buffers,
                },
                primitive,
                depth_stencil: Some(depth_state.clone()),
                multisample,
                fragment: Some(wgpu::FragmentState {
                    module: &shader,
                    entry_point: Some(entry),
                    compilation_options: wgpu::PipelineCompilationOptions::default(),
                    targets,
                }),
                multiview_mask: None,
                cache: None,
            })
        };

        let basic_opaque = make_pipeline(
            "gfx::chunk_render.basic_opaque",
            &opaque_pipeline_layout,
            "fs_main_basic_opaque",
            opaque_primitive,
            &basic_targets,
        );
        let basic_translucent = make_pipeline(
            "gfx::chunk_render.basic_translucent",
            &translucent_pipeline_layout,
            "fs_main_basic_translucent",
            translucent_primitive,
            &basic_targets,
        );
        let advanced_opaque = make_pipeline(
            "gfx::chunk_render.advanced_opaque",
            &opaque_pipeline_layout,
            "fs_main_advanced_opaque",
            opaque_primitive,
            &advanced_targets,
        );
        let advanced_translucent = make_pipeline(
            "gfx::chunk_render.advanced_translucent",
            &translucent_pipeline_layout,
            "fs_main_advanced_translucent",
            translucent_primitive,
            &advanced_targets,
        );

        Self {
            frame_layout,
            atlas_layout,
            opaque_depth_layout,
            frame_bind_group,
            atlas_bind_group,
            opaque_depth_bind_group,
            basic_opaque,
            basic_translucent,
            advanced_opaque,
            advanced_translucent,
        }
    }

    /// Recreate the group-2 (opaque depth) bind group after the
    /// G-buffer's opaque depth attachment has been recreated (resize
    /// or basic ⇄ advanced mode toggle).
    pub fn rebuild_opaque_depth_bind_group(
        &mut self,
        device: &wgpu::Device,
        gbuffer: &GBuffer,
    ) {
        self.opaque_depth_bind_group =
            build_opaque_depth_bind_group(device, &self.opaque_depth_layout, gbuffer);
    }

    /// Bind the opaque pipeline for the requested mode. The advanced
    /// variant writes MRT G-buffer; the basic variant writes the
    /// single diffuse attachment.
    pub fn begin_opaque<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>, advanced: bool) {
        let pl = if advanced {
            &self.advanced_opaque
        } else {
            &self.basic_opaque
        };
        pass.set_pipeline(pl);
        pass.set_bind_group(0, &self.frame_bind_group, &[]);
        pass.set_bind_group(1, &self.atlas_bind_group, &[]);
    }

    /// Bind the translucent pipeline for the requested mode. Both
    /// variants write into the translucent G-buffer layer with
    /// REPLACE blending (no alpha-blending — composition manually
    /// composes the two layers); the translucent depth attachment
    /// keeps the front-most translucent fragment, and a sampled
    /// opaque-depth binding lets the fragment shader discard
    /// fragments behind opaque.
    pub fn begin_translucent<'a>(&'a self, pass: &mut wgpu::RenderPass<'a>, advanced: bool) {
        let pl = if advanced {
            &self.advanced_translucent
        } else {
            &self.basic_translucent
        };
        pass.set_pipeline(pl);
        pass.set_bind_group(0, &self.frame_bind_group, &[]);
        pass.set_bind_group(1, &self.atlas_bind_group, &[]);
        pass.set_bind_group(2, &self.opaque_depth_bind_group, &[]);
    }

    /// Borrow the bind-group layout used for group 0 (frame uniforms). Useful
    /// for sibling pipelines that want to reuse the layout.
    #[must_use]
    pub fn frame_bind_group_layout(&self) -> &wgpu::BindGroupLayout {
        &self.frame_layout
    }

    /// Borrow the bind-group layout used for group 1 (atlas + sampler).
    #[must_use]
    pub fn atlas_bind_group_layout(&self) -> &wgpu::BindGroupLayout {
        &self.atlas_layout
    }
}

fn build_opaque_depth_bind_group(
    device: &wgpu::Device,
    layout: &wgpu::BindGroupLayout,
    gbuffer: &GBuffer,
) -> wgpu::BindGroup {
    device.create_bind_group(&wgpu::BindGroupDescriptor {
        label: Some("gfx::chunk_render.opaque_depth_bind_group"),
        layout,
        entries: &[wgpu::BindGroupEntry {
            binding: 0,
            resource: wgpu::BindingResource::TextureView(gbuffer.opaque_depth_view()),
        }],
    })
}

/// Re-export to make the stride visible to callers / tests without exposing
/// the `wgpu::BufferAddress` type alias.
#[must_use]
pub const fn vertex_stride() -> u64 {
    VERTEX_STRIDE
}

#[cfg(test)]
mod tests {
    use super::{VERTEX_STRIDE, vertex_stride};
    use crate::render::mesh::ChunkVertex;

    #[test]
    fn chunk_vertex_is_32_bytes() {
        assert_eq!(std::mem::size_of::<ChunkVertex>(), 32);
        assert_eq!(VERTEX_STRIDE, 32);
        assert_eq!(vertex_stride(), 32);
    }

    #[test]
    fn chunk_vertex_field_offsets_match_shader() {
        // Build one vertex and check its field byte offsets via raw pointer
        // arithmetic. These offsets are wired into the `VertexAttribute`
        // table in `ChunkPipeline::new` and must stay in sync.
        let v = ChunkVertex {
            position: [0.0, 0.0, 0.0],
            uv: [0.0, 0.0],
            layer: 0,
            face: 0,
            light: 0,
        };
        let base = std::ptr::from_ref::<ChunkVertex>(&v).cast::<u8>() as usize;
        let pos = std::ptr::from_ref(&v.position).cast::<u8>() as usize;
        let uv = std::ptr::from_ref(&v.uv).cast::<u8>() as usize;
        let layer = std::ptr::from_ref(&v.layer).cast::<u8>() as usize;
        let face = std::ptr::from_ref(&v.face).cast::<u8>() as usize;
        let light = std::ptr::from_ref(&v.light).cast::<u8>() as usize;
        assert_eq!(pos - base, 0);
        assert_eq!(uv - base, 12);
        assert_eq!(layer - base, 20);
        assert_eq!(face - base, 24);
        assert_eq!(light - base, 28);
    }
}

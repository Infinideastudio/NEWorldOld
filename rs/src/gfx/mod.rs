//! Graphics layer ([C] + [D] in `docs/rust_migration.md` §5).
//!
//! Replaces the C++ `render/*` GL wrappers and the `rendering.ixx`
//! `Renderer::` namespace with a focused `wgpu`-based pipeline:
//!
//! * [`context`] — `Gfx`: window, surface, device, queue (C1).
//! * [`basic_pipeline`] — minimal scaffold pipeline that draws a colored
//!   triangle (C2). Real chunk / UI / post pipelines plug in here later.
//! * [`atlases`] — `Atlases`: block diffuse/normal/noise + UI textures (C3).
//! * [`uniforms`] — `FrameUniforms`/`ModelUniforms`/`FilterUniforms` and a
//!   `UniformBuffer<T>` wrapper (C4).
//! * [`text`] — `TextRenderer`: glyphon-backed text rendering (C5).
//! * [`mesh`] — CPU greedy meshing per-face culling (D1).
//! * [`chunk_render`] / [`depth`] — wgpu pipeline + depth target for chunk
//!   rendering (D2 + D4).
//! * [`particle_render`] — billboard particle pipeline (D3).

pub mod atlases;
pub mod basic_pipeline;
pub mod chunk_render;
pub mod context;
pub mod depth;
pub mod mesh;
pub mod particle_render;
pub mod text;
pub mod uniforms;

pub use self::atlases::{AtlasError, Atlases};
pub use self::basic_pipeline::BasicPipeline;
pub use self::chunk_render::{ChunkMesh, ChunkPipeline};
pub use self::context::Gfx;
pub use self::depth::DepthTarget;
pub use self::mesh::{
    CHUNK_SIZE, ChunkVertex, MeshInput, MeshOutput, PADDED_SIZE, PADDED_VOLUME, mesh_chunk,
    padded_index,
};
pub use self::particle_render::{ParticleMesh, ParticlePipeline, ParticleVertex};
pub use self::text::{TextLine, TextRenderer};
pub use self::uniforms::{
    FilterUniforms, FrameUniforms, Mat4f, ModelUniforms, UniformBuffer, mat4_to_array,
};

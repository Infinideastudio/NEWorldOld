//! Graphics layer — direct mirror of the C++ `src/render/` package.
//!
//! In C++ this directory holds GL primitives (`buffer.ixx`, `program.ixx`,
//! `texture.ixx`, `vertex_array.ixx`, …) plus their layout helpers; the
//! Rust port replaces them with a focused `wgpu` pipeline. The mapping
//! isn't always one-to-one because wgpu collapses several C++ files into
//! one (e.g. `program.ixx` + `block_layout.ixx` ↔ Rust `uniforms.rs`), but
//! the file names that DO have a direct C++ neighbour live next door:
//!
//! * Texture atlas loading: see [`crate::textures`] (C++ `textures.ixx`).
//! * Glyph text rendering: see [`crate::text_rendering`] (C++ `text_rendering.ixx`).
//! * Chunk rendering pipeline: see [`crate::worlds::chunk_rendering`]
//!   (C++ `worlds/chunk_rendering.cpp`).
//!
//! Modules below are Rust-specific glue around wgpu / winit / egui that
//! doesn't have a clean C++ counterpart in `src/render/`:
//!
//! * [`context`] — `Gfx`: window, surface, device, queue (C1).
//! * [`basic_pipeline`] — minimal scaffold pipeline that draws a colored
//!   triangle (C2).
//! * [`uniforms`] — `FrameUniforms`/`ModelUniforms`/`FilterUniforms` and a
//!   `UniformBuffer<T>` wrapper (C4).
//! * [`mesh`] — CPU per-face culled meshing (D1).
//! * [`mesh_pipeline`] — off-thread mesh worker (F6).
//! * [`particle_render`] — billboard particle pipeline (D3).
//! * [`depth`] — wgpu depth attachment wrapper (D2).
//! * [`screenshot`] — surface readback to PNG (F4).
//! * [`egui_renderer`] — egui ↔ wgpu bridge for the UI layer.

pub mod basic_pipeline;
pub mod context;
pub mod depth;
pub mod egui_renderer;
pub mod mesh;
pub mod mesh_pipeline;
pub mod particle_render;
pub mod screenshot;
pub mod selection;
pub mod uniforms;

pub use self::basic_pipeline::BasicPipeline;
pub use self::context::Gfx;
pub use self::depth::DepthTarget;
pub use self::egui_renderer::EguiRenderer;
pub use self::mesh::{
    CHUNK_SIZE, ChunkVertex, MeshInput, MeshOptions, MeshOutput, PADDED_SIZE, PADDED_VOLUME,
    mesh_chunk, padded_index,
};
pub use self::mesh_pipeline::{MeshDone, MeshPipeline, MeshRequest};
pub use self::particle_render::{ParticleMesh, ParticlePipeline, ParticleVertex};
pub use self::screenshot::{Screenshot, ScreenshotError};
pub use self::selection::SelectionPipeline;
pub use self::uniforms::{
    FilterUniforms, FrameUniforms, Mat4f, ModelUniforms, UniformBuffer, mat4_to_array,
};

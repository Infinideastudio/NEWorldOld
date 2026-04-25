//! Graphics layer ([C] in `docs/rust_migration.md` §5).
//!
//! Replaces the C++ `render/*` GL wrappers and the `rendering.ixx`
//! `Renderer::` namespace with a focused `wgpu`-based pipeline. The five
//! [C] sub-tasks live here:
//!
//! * [`context`] — `Gfx`: window, surface, device, queue (C1).
//! * [`basic_pipeline`] — minimal scaffold pipeline that draws a colored
//!   triangle (C2). Real chunk / UI / post pipelines plug in here later.
//! * [`atlases`] — `Atlases`: block diffuse/normal/noise + UI textures (C3).
//! * [`uniforms`] — `FrameUniforms`/`ModelUniforms`/`FilterUniforms` and a
//!   `UniformBuffer<T>` wrapper (C4).
//! * [`text`] — `TextRenderer`: glyphon-backed text rendering (C5).

pub mod atlases;
pub mod basic_pipeline;
pub mod context;
pub mod text;
pub mod uniforms;

pub use self::atlases::{AtlasError, Atlases};
pub use self::basic_pipeline::BasicPipeline;
pub use self::context::Gfx;
pub use self::text::{TextLine, TextRenderer};
pub use self::uniforms::{FilterUniforms, FrameUniforms, Mat4f, ModelUniforms, UniformBuffer};

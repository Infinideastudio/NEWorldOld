//! `core::world` — server-safe world primitives.
//!
//! Contains the chunk page type and (eventually) the transactional page
//! store. Modules under `core/world/` own the data definitions; rendering
//! and IO modules consume them but never the other way around.

pub mod chunk;
mod chunk_generate;
mod error;
pub mod metadata;
mod page;
mod page_table;
mod pipeline;
mod txn;
mod store;
#[allow(clippy::module_inception)]
mod world;

pub use chunk::{Chunk, ChunkError};
pub use error::WorldError;
pub use metadata::WorldMetadata;
pub use pipeline::{ChunkPipeline, LoadRequest, LoadResult};
pub use store::TilesStore;
pub use world::*;

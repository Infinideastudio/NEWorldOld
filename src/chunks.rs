//! Re-export of [`crate::core::world::chunk`] under the legacy
//! `crate::chunks` path. Kept while callers migrate.

pub use crate::core::world::Chunk;
pub use crate::core::world::chunk::{Blocks, ChunkError, SIZE, SIZE_CUBED, SIZE_DATA, SIZE_LOG, SIZE_USIZE};

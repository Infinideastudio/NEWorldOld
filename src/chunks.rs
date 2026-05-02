//! Re-export of [`crate::core::world::chunk`] under the legacy `crate::chunks`
//! path. The chunk type moved into `core/world/` once the simulation half of
//! the engine was carved out — `Chunk`, `ChunkError`, and the save-format
//! constants are all server-safe with no GPU dependencies. Existing call
//! sites that say `use crate::chunks::Chunk` keep working through this shim.

pub use crate::core::world::chunk::*;

//! Re-export of [`crate::core::worldgen::generator`] under the legacy
//! `crate::terrain_generation` path. The generator moved into
//! `core/worldgen/` once the simulation half of the engine was carved out.
//! Existing call sites that say `use crate::terrain_generation::Generator`
//! keep working through this shim.

pub use crate::core::worldgen::generator::*;

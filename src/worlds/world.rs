//! Re-export of [`crate::core::world`] under the legacy `crate::worlds::world`
//! path. The world types moved into `core/world/` once the simulation half
//! of the engine was carved out. Existing call sites that say
//! `use crate::worlds::world::World` keep working through this shim.

pub use crate::core::world::*;

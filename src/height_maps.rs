//! Re-export of [`crate::core::worldgen::height_map`] under the legacy
//! `crate::height_maps` path. The height-cache moved into `core/worldgen/`
//! alongside the generator. Existing call sites that say
//! `use crate::height_maps::HeightMap` keep working through this shim.

pub use crate::core::worldgen::height_map::*;

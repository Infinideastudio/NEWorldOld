//! Core gameplay primitives — block, chunk, and world types decoupled from
//! rendering. Modules under `core/` own the data definitions; rendering and
//! IO modules consume them but never the other way around.

pub mod blocks;
pub mod game;
pub mod math;
pub mod world;

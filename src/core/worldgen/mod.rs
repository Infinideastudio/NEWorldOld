//! `core::worldgen` — terrain generation and height maps.
//!
//! Pure content rules expressed against `core::world`'s page-store API.
//! No GPU dependencies; runs on the server side too.

pub mod generator;
pub mod height_map;

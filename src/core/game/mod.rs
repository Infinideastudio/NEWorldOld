//! `core::game` — server-safe gameplay logic that *uses* `core::world`.
//!
//! Player state, block-update queues, random ticks, terrain-generation
//! rules, and the streaming policy that pins the loaded core all live
//! here. None of this code talks to wgpu or egui; it's the half of the
//! engine that a future headless server build links against.

pub mod block_update;
pub mod commands;
pub mod daylight_cycle;
pub mod player;
pub mod range_loader;
pub mod worldgen;

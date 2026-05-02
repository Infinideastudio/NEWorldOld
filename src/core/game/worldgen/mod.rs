//! `core::game::worldgen` — terrain generation: rules + state.
//!
//! Two layers, both registry-aware:
//!
//! - **Rules** ([`HeightNoise`], [`init_generate`]): pure functions
//!   that map coords + seed + `BaseBlocks` to chunk contents.
//!   Stateless from the caller's perspective beyond the seed they
//!   were built with.
//! - **Orchestrator** ([`TerrainGenerator`]): owns the chunk pipeline
//!   worker thread, the seeded noise, and the resolved `BaseBlocks`.
//!   Provides `request_load` / `drain_results` / `request_save` for
//!   async I/O and `build_chunk_sync` for the blocking warm-up paths
//!   used by tests.
//!
//! Lives outside `core::world` because everything here knows about
//! `BaseBlocks` and the `BlockRegistry` — neither of which the
//! database half should depend on.

mod chunk_init;
pub mod noise;
mod terrain_generator;

pub use chunk_init::init_generate;
pub use noise::{HeightNoise, WATER_LEVEL};
pub use terrain_generator::{TerrainGenerator, world_tables_for};

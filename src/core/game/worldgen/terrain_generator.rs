//! `TerrainGenerator` — owns the chunk pipeline + the main-thread
//! sync-fallback gen state.
//!
//! Lives outside `World` because the database doesn't know about
//! `BaseBlocks`, the block registry, or terrain rules. It only knows
//! about chunk *bytes* and the canonical-id translation tables.
//! `TerrainGenerator` is the registry-aware counterpart that turns
//! coords into chunks, either by reading sled (registry-translation
//! handled per-world) or by running the [`HeightNoise`] / chunk-init
//! rules.
//!
//! Owns:
//! - `pipeline: ChunkPipeline` — the worker thread that does async
//!   load (sled fetch + unpackage / fall-through-to-gen) and async
//!   save (sled insert).
//! - `noise: HeightNoise` — terrain height noise, cloned into the
//!   worker on spawn.
//! - `base_blocks: BaseBlocks` — the resolved id table for terrain
//!   layers (rock, dirt, grass, water, sand, bedrock, air).
//! - `registry: Arc<BlockRegistry>` — kept so future sync paths can
//!   look up by name; not consulted today.

use std::path::Path;
use std::sync::Arc;

use crate::blocks::{BaseBlocks, BlockRegistry, Light};
use crate::core::game::pipeline::{ChunkPipeline, LoadResult};
use crate::core::world::{Blocks, Metadata, World, WorldError, WorldTables};
use crate::math::Vec3i;

use super::HeightNoise;
use super::chunk_init;

pub struct TerrainGenerator {
    pipeline: ChunkPipeline,
    noise: HeightNoise,
    base_blocks: BaseBlocks,
    #[allow(dead_code)]
    registry: Arc<BlockRegistry>,
}

/// Build the [`WorldTables`] for the world named `name` rooted at
/// `root` against the in-memory `registry`. Loads `world.dat` if
/// present (translating canonical ↔ current ids); otherwise
/// snapshots from `registry`. Identity tables collapse to empty
/// `Vec`s so the chunk codec can take the fast path.
pub fn world_tables_for(
    root: &Path,
    name: &str,
    registry: &BlockRegistry,
) -> Result<WorldTables, WorldError> {
    let dir = root.join("worlds").join(name);
    let metadata_path = dir.join("world.dat");
    if metadata_path.exists() {
        let metadata = Metadata::load_from(&metadata_path)?;
        let load = metadata.canonical_to_current(registry);
        let save = metadata.current_to_canonical(registry);
        let load_identity = load.len() == registry.len()
            && load.iter().enumerate().all(|(i, id)| id.0 as usize == i);
        let save_identity =
            save.len() == registry.len() && save.iter().enumerate().all(|(i, &c)| c as usize == i);
        Ok(WorldTables {
            metadata,
            load_table: if load_identity { Vec::new() } else { load },
            save_table: if save_identity { Vec::new() } else { save },
        })
    } else {
        Ok(WorldTables {
            metadata: Metadata::from_registry(registry),
            load_table: Vec::new(),
            save_table: Vec::new(),
        })
    }
}

impl TerrainGenerator {
    /// Construct a `TerrainGenerator` against `world`'s sled store.
    /// The pipeline worker is spawned with its own clones of the
    /// height noise, base blocks, sled handle, and the canonical-id
    /// translation table.
    pub fn new(
        world: &World,
        registry: Arc<BlockRegistry>,
        base_blocks: BaseBlocks,
        seed: u32,
    ) -> Self {
        let noise = HeightNoise::new(seed);
        let load_table = Arc::new(world.chunk_load_table().to_vec());
        let pipeline = ChunkPipeline::spawn(
            Arc::clone(&registry),
            base_blocks,
            noise.clone(),
            world.db_handle(),
            load_table,
        );
        Self {
            pipeline,
            noise,
            base_blocks,
            registry,
        }
    }

    // ---- async pipeline -------------------------------------------------

    /// Issue an async load for `coord`. Returns true if the request
    /// landed on the worker queue.
    pub fn request_load(&self, coord: Vec3i) -> bool {
        self.pipeline.request_load(coord)
    }

    /// Drain every available `LoadResult` from the worker.
    pub fn drain_results(&self) -> Vec<LoadResult> {
        self.pipeline.drain_results()
    }

    /// Fire-and-forget save through the worker.
    pub fn request_save(&self, coord: Vec3i, bytes: Vec<u8>) {
        self.pipeline.request_save(coord, bytes);
    }

    // ---- sync helper ----------------------------------------------------

    /// Build a chunk for `coord` synchronously. Tries `world`'s
    /// on-disk store first (using `world`'s id translation table); on
    /// miss, runs terrain generation. Returns `(blocks, from_disk)`.
    pub fn build_chunk_sync(&mut self, world: &World, coord: Vec3i) -> (Blocks, bool) {
        let default_light = if coord.y < 0 { Light::NONE } else { Light::SKY };
        let mut blocks = Blocks::air_filled(default_light);
        let from_disk = match world.load_raw_bytes(coord) {
            Some(bytes) => blocks
                .unpackage_from(&bytes, world.chunk_load_table())
                .is_ok(),
            None => false,
        };
        if !from_disk {
            chunk_init::init_generate(&mut blocks, coord, &self.noise, &self.base_blocks);
        }
        (blocks, from_disk)
    }

}

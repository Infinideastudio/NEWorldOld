//! `World` — the chunk-store database, registry-agnostic.
//!
//! Owns a `DashMap<Vec3i, Arc<Chunk>>` plus the per-world canonical
//! id translation tables and the world's directory + sled store.
//! Knows nothing about the block registry, base blocks, terrain
//! generation, the player, or the game clock — those all live in
//! `core::game::*` modules and consume `World` via its small,
//! number-only API.
//!
//! What lives elsewhere:
//! - `RangeLoader` (`core::game::range_loader`) — streaming policy +
//!   pin set + load window.
//! - `TerrainGenerator` (`core::game::worldgen`) — the chunk pipeline,
//!   terrain rules, base-block id table.
//! - `BlockUpdateQueue` + write/tick functions
//!   (`core::game::block_update`) — gameplay mutators.
//! - `DaylightCycle` (`core::game::daylight_cycle`) — game-time clock.
//! - `Player` (`core::game::player`) — gameplay state. Owned by `Game`.

use std::collections::HashSet;
use std::path::{Path, PathBuf};
use std::sync::Arc;

use dashmap::DashMap;

use crate::blocks::{BlockData, BlockId, Light, State};
use crate::math::{Vec3i, Vec3u};

use super::chunk::{self, Blocks, Chunk};
use super::error::WorldError;
use super::metadata::Metadata;
use super::store::Store;
use super::txn::{self, TxnError, WorkingSet};

// ----------------------------------------------------------------------
//   Coord helpers
// ----------------------------------------------------------------------

/// Floor-divide a block coord by chunk size to get the containing
/// chunk coord.
pub fn chunk_coord(coord: Vec3i) -> Vec3i {
    Vec3i::new(
        coord.x >> chunk::SIZE_LOG,
        coord.y >> chunk::SIZE_LOG,
        coord.z >> chunk::SIZE_LOG,
    )
}

/// Block-local coord (within its chunk) for a global block coord.
pub fn block_coord(coord: Vec3i) -> Vec3u {
    let mask = chunk::SIZE - 1;
    Vec3u::new(
        (coord.x & mask) as u32,
        (coord.y & mask) as u32,
        (coord.z & mask) as u32,
    )
}

// ----------------------------------------------------------------------
//   Construction tables
// ----------------------------------------------------------------------

/// Canonical id translation tables for a world. Built by the
/// registry-aware caller from the world's `world.dat` (or freshly
/// snapshotted from the registry on first run), then handed to
/// [`World::new_at`].
pub struct WorldTables {
    pub metadata: Metadata,
    /// Translation applied when loading a chunk: indexed by canonical
    /// id, gives the in-memory `BlockId`. Empty `Vec` means identity.
    pub load_table: Vec<BlockId>,
    /// Translation applied when saving a chunk: indexed by in-memory
    /// id, gives the canonical id (`u16`). Empty `Vec` means identity.
    pub save_table: Vec<u16>,
}

// ----------------------------------------------------------------------
//   World
// ----------------------------------------------------------------------

pub struct World {
    name: String,
    dir: PathBuf,
    store: Store,
    metadata: Metadata,
    chunk_load_table: Vec<BlockId>,
    chunk_save_table: Vec<u16>,
    /// Sharded `Vec3i → Arc<Chunk>` map. Lookups clone the `Arc<Chunk>`
    /// and drop the shard guard immediately. The `Arc` itself is the
    /// pin: eviction drops the world's clone, while txn entries hold
    /// their own clones to keep the chunk alive.
    chunks: DashMap<Vec3i, Arc<Chunk>>,
    /// Mirror of `chunks`'s key set — DashMap iteration would hold
    /// shard guards; this lets the streaming layer walk coords
    /// lock-free.
    coords: HashSet<Vec3i>,
    pub unloaded_chunks: u32,
    pub updated_blocks: u32,
}

impl World {
    // ---- construction ---------------------------------------------------

    /// Create or open the world named `name` rooted at
    /// `<root>/worlds/<name>/`. The caller (registry-aware) provides
    /// pre-built [`WorldTables`] — see
    /// [`crate::core::game::worldgen::world_tables_for`].
    pub fn new_at(root: &Path, name: String, tables: WorldTables) -> Result<Self, WorldError> {
        let dir = root.join("worlds").join(&name);
        std::fs::create_dir_all(&dir)?;
        let store = Store::open_at(&dir.join("chunks.db"))?;
        Ok(Self {
            name,
            dir,
            store,
            metadata: tables.metadata,
            chunk_load_table: tables.load_table,
            chunk_save_table: tables.save_table,
            chunks: DashMap::new(),
            coords: HashSet::new(),
            unloaded_chunks: 0,
            updated_blocks: 0,
        })
    }

    // ---- identity -------------------------------------------------------

    pub fn name(&self) -> &str {
        &self.name
    }

    /// World directory on disk. Callers that need per-world auxiliary
    /// files (player save, etc.) build paths off this.
    pub fn dir(&self) -> &Path {
        &self.dir
    }

    /// Cheap-clonable handle to the underlying sled DB. Used by the
    /// chunk pipeline worker thread, which writes/reads chunk bytes
    /// directly without going through the main thread.
    pub fn db_handle(&self) -> Arc<sled::Db> {
        self.store.db_handle()
    }

    // ---- translation tables --------------------------------------------

    pub fn chunk_load_table(&self) -> &[BlockId] {
        &self.chunk_load_table
    }

    pub fn chunk_save_table(&self) -> &[u16] {
        &self.chunk_save_table
    }

    // ---- chunk presence / iteration -------------------------------------

    pub fn is_loaded(&self, ccoord: Vec3i) -> bool {
        self.coords.contains(&ccoord)
    }

    pub fn loaded_count(&self) -> usize {
        self.coords.len()
    }

    /// Iterate every loaded chunk coord. Lock-free — walks the
    /// in-memory `coords` mirror, not the DashMap.
    pub fn loaded_iter(&self) -> impl Iterator<Item = Vec3i> + '_ {
        self.coords.iter().copied()
    }

    /// Look up the chunk at `ccoord`. Clones the `Arc<Chunk>` out of
    /// the DashMap and drops the shard guard immediately, so the
    /// caller can hold the returned Arc across awaits / locks.
    /// Holding the returned Arc also pins the chunk: eviction may drop
    /// the world's clone, but the chunk lives until every external
    /// `Arc<Chunk>` is dropped.
    pub fn chunk(&self, ccoord: Vec3i) -> Option<Arc<Chunk>> {
        self.chunks.get(&ccoord).map(|r| Arc::clone(r.value()))
    }

    /// Direct access to the chunk map for the [`txn`] internals.
    pub(super) fn chunks(&self) -> &DashMap<Vec3i, Arc<Chunk>> {
        &self.chunks
    }

    // ---- transactional read/write entry points --------------------------

    pub fn begin_read_txn_sync(
        &self,
        working_set: impl Into<WorkingSet>,
    ) -> Result<super::txn::ReadTxn, TxnError> {
        txn::begin_read_sync(self, working_set.into())
    }

    pub fn begin_write_txn_sync(
        &self,
        working_set: impl Into<WorkingSet>,
    ) -> Result<super::txn::WriteTxn, TxnError> {
        txn::begin_write_sync(self, working_set.into())
    }

    // ---- single-coord block lookup --------------------------------------

    /// Single-coord block lookup. Routes through a 1-coord `ReadTxn`.
    /// **Inefficient on hot paths** — one DashMap lookup, Arc clone,
    /// and lock acquire per call. For multi-cell reads against the
    /// same chunk(s), open one `ReadTxn` and reuse it.
    pub fn block(&self, coord: Vec3i) -> Option<BlockData> {
        let cc = chunk_coord(coord);
        let txn = self.begin_read_txn_sync(WorkingSet::Single(cc)).ok()?;
        txn.read(coord).ok()
    }

    /// Single-coord block lookup with an air fallback.
    /// **Inefficient on hot paths** — see [`Self::block`].
    pub fn block_or_air(&self, coord: Vec3i) -> BlockData {
        self.block(coord).unwrap_or(BlockData {
            id: BlockId::EMPTY,
            state: State::default(),
            light: Light::NONE,
        })
    }

    // ---- mesh-dirty tracking --------------------------------------------

    /// Iterate coords whose mesh-dirty atomic is set.
    pub fn drain_updated_chunks(&self) -> Vec<Vec3i> {
        let mut out = Vec::new();
        for cc in self.loaded_iter() {
            if let Some(p) = self.chunk(cc)
                && p.updated()
            {
                out.push(cc);
            }
        }
        out
    }

    pub fn clear_updated_chunks(&self, coords: &[Vec3i]) {
        for &cc in coords {
            if let Some(p) = self.chunk(cc) {
                p.clear_updated();
            }
        }
    }

    /// Mark every loaded chunk's mesh-dirty atomic. Used to force a
    /// full re-mesh after meshing rules flip.
    pub fn mark_all_loaded_for_remesh(&self) {
        for cc in self.loaded_iter() {
            if let Some(p) = self.chunk(cc) {
                p.mark_updated();
            }
        }
    }

    /// Mark every chunk in the 3×3×3 cube around `ccoord` as needing
    /// a re-mesh. Called after a chunk lands so neighbouring chunks
    /// re-mesh against the real blocks.
    pub fn mark_neighbour_chunks_updated(&self, ccoord: Vec3i) {
        for dz in -1..=1 {
            for dy in -1..=1 {
                for dx in -1..=1 {
                    let target = ccoord + Vec3i::new(dx, dy, dz);
                    if let Some(p) = self.chunk(target) {
                        p.mark_updated();
                    }
                }
            }
        }
    }

    // ---- chunk install / evict / writeback ------------------------------

    /// Install a freshly-loaded chunk's blocks. `from_disk` selects
    /// the initial `(save_gen, commit_gen)` pair: `(1, 1)` from sled,
    /// `(0, 1)` from the generator.
    pub fn install_chunk(&mut self, ccoord: Vec3i, blocks: Blocks, from_disk: bool) {
        let chunk = Arc::new(if from_disk {
            Chunk::from_disk(ccoord, blocks)
        } else {
            Chunk::from_gen(ccoord, blocks)
        });
        match self.chunks.entry(ccoord) {
            dashmap::mapref::entry::Entry::Occupied(_) => {
                // Race lost; discard the new chunk, keep the existing.
            }
            dashmap::mapref::entry::Entry::Vacant(slot) => {
                slot.insert(chunk);
                self.coords.insert(ccoord);
            }
        }
    }

    /// Drop the chunk at `ccoord` from the map. The Arc may stay alive
    /// in callers' clones; the slot just becomes unreachable for
    /// future lookups.
    pub fn evict(&mut self, ccoord: Vec3i) {
        if self.chunks.remove(&ccoord).is_some() {
            self.coords.remove(&ccoord);
        }
    }

    /// If the chunk at `ccoord` is dirty (`save_gen < commit_gen`),
    /// package it through a 1-coord `ReadTxn` and capture the gen the
    /// snapshot represents. Returns `(bytes, captured_gen)`.
    pub fn package_if_dirty(&self, ccoord: Vec3i) -> Option<(Vec<u8>, u64)> {
        let chunk = self.chunk(ccoord)?;
        if !chunk.dirty() {
            return None;
        }
        let txn = txn::begin_read_sync(self, WorkingSet::Single(ccoord)).ok()?;
        let captured_gen = chunk.commit_gen();
        let blocks = txn.chunk_at(ccoord)?;
        let bytes = blocks.package_to(&self.chunk_save_table);
        drop(txn);
        Some((bytes, captured_gen))
    }

    /// Advance the chunk's `save_gen` to `captured_gen` (monotonic
    /// `fetch_max`). Called after a successful disk write of the
    /// bytes returned by [`Self::package_if_dirty`].
    pub fn advance_save_gen(&self, ccoord: Vec3i, captured_gen: u64) {
        if let Some(c) = self.chunk(ccoord) {
            c.advance_save_gen(captured_gen);
        }
    }

    // ---- raw byte I/O ---------------------------------------------------

    /// Sync read of the on-disk bytes for `coord`. `None` if no
    /// persisted copy. Registry-agnostic.
    pub fn load_raw_bytes(&self, coord: Vec3i) -> Option<Vec<u8>> {
        match self.store.load(coord) {
            Ok(Some(bytes)) => Some(bytes),
            _ => None,
        }
    }

    /// Sync write of `bytes` to the on-disk store for `coord`.
    /// Registry-agnostic.
    pub fn save_chunk_bytes(&self, coord: Vec3i, bytes: &[u8]) -> Result<(), WorldError> {
        self.store.save(coord, bytes)?;
        Ok(())
    }

    // ---- save / housekeeping --------------------------------------------

    /// Persist every dirty chunk to disk and write `world.dat`. Does
    /// not save the player — that's the caller's responsibility (the
    /// player lives outside `World`).
    pub fn save_to_disk(&mut self) -> Result<(), WorldError> {
        let coords: Vec<Vec3i> = self.loaded_iter().collect();
        for cc in coords {
            if let Some((bytes, captured_gen)) = self.package_if_dirty(cc) {
                self.store.save(cc, &bytes)?;
                self.advance_save_gen(cc, captured_gen);
            }
        }
        self.store.flush()?;
        std::fs::create_dir_all(&self.dir)?;
        self.metadata.save_to(&self.dir.join("world.dat"))?;
        Ok(())
    }
}

// ----------------------------------------------------------------------
//   World-directory helpers (don't need a `World` instance)
// ----------------------------------------------------------------------

/// List the names of every directory under `<root>/worlds/`.
pub fn list_worlds_at(root: &Path) -> Vec<String> {
    let worlds_dir = root.join("worlds");
    let Ok(entries) = std::fs::read_dir(&worlds_dir) else {
        return Vec::new();
    };
    let mut names: Vec<String> = entries
        .filter_map(Result::ok)
        .filter(|e| e.file_type().is_ok_and(|t| t.is_dir()))
        .filter_map(|e| e.file_name().into_string().ok())
        .collect();
    names.sort();
    names
}

/// Recursively delete `<root>/worlds/<name>/`. No-op if the directory
/// doesn't exist.
pub fn delete_world_at(root: &Path, name: &str) -> Result<(), std::io::Error> {
    let dir = root.join("worlds").join(name);
    if !dir.exists() {
        return Ok(());
    }
    std::fs::remove_dir_all(&dir)
}

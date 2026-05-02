//! `World` — the chunk-store database, registry-agnostic.
//!
//! Owns a `DashMap<Vec3i, Arc<Chunk>>` plus the per-world canonical
//! id translation tables and the world's directory + sled store.
//! Knows nothing about the block registry, base blocks, terrain
//! generation, the player, or the game clock — those all live in
//! `core::game::*` modules and consume `World` via its small,
//! number-only API.
//!
//! **Storage encapsulation.** The sled-backed [`Store`] is a private
//! implementation detail of `World`; nothing outside this module can
//! reach it. Disk-load on chunk install, dirty-flush on eviction,
//! the periodic [`World::sweep_dirty`] sweep, and the `save_to_disk`
//! fence all run through `World`.
//!
//! **Save policy.**
//! - [`World::load_chunk`] tries to load the chunk from disk first;
//!   only on miss does it call the caller-supplied generator closure.
//! - [`World::unload_chunk`] drops a chunk from memory; persists it
//!   if it has uncommitted changes.
//! - [`World::sweep_dirty`] flushes up to a budget of dirty chunks
//!   each call; intended to be called periodically so on-quit save
//!   has little remaining work.
//! - [`World::save_to_disk`] is the durability fence used by autosave
//!   and on-quit.
//!
//! Renderer-side mesh-dirty hooks (`mark_neighbour_chunks_updated`,
//! `drain_updated_chunks`, `clear_updated_chunks`,
//! `mark_all_loaded_for_remesh`) currently live on `World` but are
//! not part of the database interpretation; a future round will move
//! them to a renderer-side dirty-set.

mod chunk;
mod error;
mod metadata;
mod store;
mod txn;

pub use chunk::{Chunk, ChunkData};
pub use error::WorldError;
pub use metadata::Metadata;
pub use store::Store;
pub use txn::{ReadTxn, TxnError, WorkingSet, WriteTxn};

use std::path::{Path, PathBuf};
use std::sync::Arc;

use dashmap::DashMap;

use crate::core::blocks::{BlockData, BlockId, BlockLight, BlockState};
use crate::core::math::{Vec3i, Vec3u};

// ----------------------------------------------------------------------
//   Coord helpers
// ----------------------------------------------------------------------

/// Floor-divide a block coord by chunk size to get the containing
/// chunk coord.
pub fn chunk_coord(coord: Vec3i) -> Vec3i {
    Vec3i::new(
        coord.x >> Chunk::SIZE_LOG,
        coord.y >> Chunk::SIZE_LOG,
        coord.z >> Chunk::SIZE_LOG,
    )
}

/// Block-local coord (within its chunk) for a global block coord.
pub fn block_coord(coord: Vec3i) -> Vec3u {
    let mask = (Chunk::SIZE - 1) as u32;
    Vec3u::new(
        coord.x as u32 & mask,
        coord.y as u32 & mask,
        coord.z as u32 & mask,
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
}

impl World {
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
        })
    }

    /// List the names of every directory under `<root>/worlds/`.
    pub fn list_at(root: &Path) -> Vec<String> {
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
    pub fn delete_at(root: &Path, name: &str) -> Result<(), std::io::Error> {
        let dir = root.join("worlds").join(name);
        if !dir.exists() {
            return Ok(());
        }
        std::fs::remove_dir_all(&dir)
    }

    // ---- identity -------------------------------------------------------

    pub fn name(&self) -> &str {
        &self.name
    }

    pub fn dir(&self) -> &Path {
        &self.dir
    }

    // ---- chunk presence / iteration -------------------------------------

    pub fn is_loaded(&self, ccoord: Vec3i) -> bool {
        self.chunks.contains_key(&ccoord)
    }

    pub fn loaded_count(&self) -> usize {
        self.chunks.len()
    }

    pub fn loaded_coords(&self) -> Vec<Vec3i> {
        self.chunks.iter().map(|r| *r.key()).collect()
    }

    /// Look up the chunk at `ccoord`. Holding the returned Arc pins
    /// the chunk against eviction.
    pub(super) fn chunk(&self, ccoord: Vec3i) -> Option<Arc<Chunk>> {
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
    ) -> Result<txn::ReadTxn, TxnError> {
        txn::begin_read_sync(self, working_set.into())
    }

    pub fn begin_write_txn_sync(
        &self,
        working_set: impl Into<WorkingSet>,
    ) -> Result<txn::WriteTxn, TxnError> {
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
            state: BlockState::default(),
            light: BlockLight::NONE,
        })
    }

    // ---- chunk load / unload --------------------------------------------

    /// Loads the chunk at `ccoord`. World tries its on-disk store
    /// first; on miss, calls `init` to produce a fresh chunk.
    /// The closure is consumed only on disk-miss, so callers can
    /// hand in pre-generated blocks (e.g. from an async worker)
    /// without wasting them on hits.
    ///
    /// No-op if a chunk is already loaded at `ccoord` (race-safe).
    pub fn load_chunk<F>(&mut self, ccoord: Vec3i, init: F)
    where
        F: FnOnce() -> ChunkData,
    {
        match self.chunks.entry(ccoord) {
            dashmap::mapref::entry::Entry::Occupied(_) => {}
            dashmap::mapref::entry::Entry::Vacant(slot) => {
                let chunk = match self.try_load_from_disk(ccoord) {
                    Ok(Some(blocks)) => Chunk::from_disk(blocks),
                    Ok(None) => Chunk::from_gen(init()),
                    Err(err) => {
                        tracing::warn!(?ccoord, error = %err, "chunk disk load failed; regenerating");
                        Chunk::from_gen(init())
                    }
                };
                slot.insert(Arc::new(chunk));
            }
        }
    }

    /// Drop the chunk at `ccoord` from the map. If the chunk has
    /// uncommitted changes, persists them synchronously first; on
    /// I/O failure the chunk is still removed and the error is
    /// logged. The Arc may stay alive in callers' clones; the slot
    /// just becomes unreachable for future lookups.
    pub fn unload_chunk(&mut self, ccoord: Vec3i) {
        if let Err(err) = self.flush_chunk(ccoord) {
            tracing::warn!(?ccoord, error = %err, "chunk save on unloading failed");
        }
        self.chunks.remove(&ccoord);
    }

    // ---- save / housekeeping --------------------------------------------

    /// Flush at most `budget` dirty chunks to disk. Returns the
    /// number actually written. Intended for periodic background
    /// calls so [`Self::save_to_disk`] on quit has little to do.
    /// Errors during individual flushes are logged and swallowed —
    /// the sweep keeps going and surfaces nothing to the caller.
    pub fn sweep_dirty(&self, budget: usize) -> usize {
        if budget == 0 {
            return 0;
        }
        // Collect dirty coords first so we don't hold shard guards
        // across the per-chunk flush (which itself locks).
        let mut dirty: Vec<Vec3i> = Vec::with_capacity(budget);
        for r in self.chunks.iter() {
            if dirty.len() >= budget {
                break;
            }
            if r.value().dirty() {
                dirty.push(*r.key());
            }
        }
        let mut written = 0;
        for cc in dirty {
            match self.flush_chunk(cc) {
                Ok(()) => written += 1,
                Err(err) => tracing::warn!(?cc, error = %err, "sweep flush failed"),
            }
        }
        written
    }

    /// Persist every dirty chunk to disk and write `world.dat`. Does
    /// not save the player — that's the caller's responsibility (the
    /// player lives outside `World`).
    pub fn save_to_disk(&mut self) -> Result<(), WorldError> {
        for cc in self.loaded_coords() {
            self.flush_chunk(cc)?;
        }
        self.store.flush()?;
        self.metadata.save_to(&self.dir.join("world.dat"))?;
        Ok(())
    }

    // ---- private helpers ------------------------------------------------

    /// Try to deserialize the chunk at `coord` from sled. `Ok(None)`
    /// means there is no on-disk copy.
    fn try_load_from_disk(&self, coord: Vec3i) -> Result<Option<ChunkData>, WorldError> {
        let Some(bytes) = self.store.load(coord)? else {
            return Ok(None);
        };
        let default_light = if coord.y < 0 {
            BlockLight::NONE
        } else {
            BlockLight::SKY
        };
        let mut blocks = ChunkData::air_filled(default_light);
        match blocks.unpackage_from(&bytes, &self.chunk_load_table) {
            Ok(()) => Ok(Some(blocks)),
            Err(err) => {
                tracing::warn!(?coord, error = %err, "chunk unpackage failed; regenerating");
                Ok(None)
            }
        }
    }

    /// Snapshot the chunk at `ccoord`, write it to sled if dirty, and
    /// advance its persisted-state counter. No-op if the chunk isn't
    /// loaded or isn't dirty.
    fn flush_chunk(&self, ccoord: Vec3i) -> Result<(), WorldError> {
        let Some(chunk) = self.chunk(ccoord) else {
            return Ok(());
        };
        if !chunk.dirty() {
            return Ok(());
        }
        let txn = self
            .begin_read_txn_sync(WorkingSet::Single(ccoord))
            .map_err(|_| WorldError::Io(std::io::Error::other("chunk gone before flush")))?;
        let captured_gen = chunk.commit_gen();
        let blocks = match txn.chunk_at(ccoord) {
            Some(b) => b,
            None => return Ok(()),
        };
        let bytes = blocks.package_to(&self.chunk_save_table);
        drop(txn);
        self.store.save(ccoord, &bytes)?;
        chunk.advance_save_gen(captured_gen);
        Ok(())
    }

    // ---- mesh-dirty tracking (TODO: factor out to render) ---------------

    /// Renderer hook: snapshot of coords whose mesh-dirty atomic is set.
    pub fn drain_updated_chunks(&self) -> Vec<Vec3i> {
        self.chunks
            .iter()
            .filter(|r| r.value().updated())
            .map(|r| *r.key())
            .collect()
    }

    /// Renderer hook: clear the mesh-dirty atomic on `coords` (called
    /// by the renderer after it dispatches a remesh for each).
    pub fn clear_updated_chunks(&self, coords: &[Vec3i]) {
        for &cc in coords {
            if let Some(p) = self.chunk(cc) {
                p.clear_updated();
            }
        }
    }

    /// Renderer hook: mark every loaded chunk's mesh-dirty atomic.
    /// Used to force a full re-mesh after meshing rules flip.
    pub fn mark_all_loaded_for_remesh(&self) {
        for r in self.chunks.iter() {
            r.value().mark_updated();
        }
    }

    /// Renderer hook: mark every chunk in the 3×3×3 cube around
    /// `ccoord` as needing a re-mesh. Called after a chunk lands so
    /// neighbouring chunks re-mesh against the real blocks.
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
}

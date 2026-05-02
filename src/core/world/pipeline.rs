//! Async chunk load/save pipeline ([F5] in `docs/rust_migration.md` §5).
//!
//! Spawns one background worker thread that owns a clone of the
//! `Arc<sled::Db>` plus its own `Generator`, `BaseBlocks`,
//! `Arc<BlockRegistry>`, and `HeightMap`. The main thread sends
//! [`LoadRequest`]s through a `crossbeam-channel`; the worker
//! generates / loads-from-sled the chunk and ships the finished
//! [`LoadResult`] back. Save requests are fire-and-forget.
//!
//! ## `HeightMap` strategy
//!
//! The worker carries its **own** `HeightMap` instance. Per the migration
//! plan §4.6, `HeightMap` is not thread-safe — it's a sliding-window cache
//! mutated during `init_generate`. The simplest correct option is per-worker
//! ownership: load requests are processed serially on the worker, so a single
//! `HeightMap` there suffices, and the main thread's height map stays
//! independent of the async path.
//!
//! ## Cross-thread chunk identity
//!
//! Every cross-thread reference is a coord. The main thread re-resolves
//! load results through the world's hash map after the worker returns —
//! so a chunk that was unloaded mid-flight is dropped instead of revived.

use std::sync::Arc;
use std::thread::JoinHandle;

use crossbeam_channel::{Receiver, Sender, unbounded};

use crate::blocks::{BaseBlocks, BlockId, BlockRegistry};
use super::chunk::Chunk;
use crate::height_maps::HeightMap;
use crate::math::Vec3i;
use crate::terrain_generation::Generator;

/// One unit of work for the chunk pipeline worker.
pub enum LoadRequest {
    /// Build a fresh chunk for `coord`: try sled first, fall back to terrain
    /// generation if no bytes are stored.
    Load(Vec3i),
    /// Persist `bytes` for `coord` to sled. Fire-and-forget — failures are
    /// logged but not surfaced to the caller.
    Save(Vec3i, Vec<u8>),
}

/// Result of a successful [`LoadRequest::Load`]. `chunk` is fully initialized
/// (post-`init_generate` + `post_init`) and ready for `World::insert`.
pub struct LoadResult {
    pub coord: Vec3i,
    pub chunk: Chunk,
}

/// Owner-side handle for the load/save worker. Drops the request channel on
/// `drop`, signalling the worker to exit; joins the thread.
pub struct ChunkPipeline {
    /// Wrapped in `Option` so `drop` can take it, closing the channel and
    /// signalling the worker to exit before `join`.
    requests: Option<Sender<LoadRequest>>,
    results: Receiver<LoadResult>,
    handle: Option<JoinHandle<()>>,
}

impl ChunkPipeline {
    /// Spawn the worker thread. The worker owns its own `HeightMap`; the
    /// `Generator`, `BaseBlocks`, registry and sled DB handle are shared with
    /// the main thread via cheap clones.
    ///
    /// `canonical_to_current` is the per-world id translation: indexed by
    /// the canonical id stored on disk, each entry is the in-memory
    /// `BlockId` to substitute when loading. Pass an empty `Vec` for an
    /// identity mapping (the world's canonical mapping equals the current
    /// registry — typical for a freshly-created world).
    #[must_use]
    pub fn spawn(
        registry: Arc<BlockRegistry>,
        base_blocks: BaseBlocks,
        generator: Generator,
        db: Arc<sled::Db>,
        height_map_size: usize,
        canonical_to_current: Arc<Vec<BlockId>>,
    ) -> Self {
        let (req_tx, req_rx) = unbounded::<LoadRequest>();
        let (res_tx, res_rx) = unbounded::<LoadResult>();
        let handle = std::thread::Builder::new()
            .name("neworld-chunk-pipeline".into())
            .spawn(move || {
                worker_loop(
                    req_rx,
                    res_tx,
                    registry,
                    base_blocks,
                    generator,
                    db,
                    height_map_size,
                    canonical_to_current,
                );
            })
            .expect("spawn chunk pipeline worker");
        Self {
            requests: Some(req_tx),
            results: res_rx,
            handle: Some(handle),
        }
    }

    /// Send a [`LoadRequest::Load`]. Returns `false` if the channel is closed
    /// (worker has exited).
    #[must_use]
    pub fn request_load(&self, coord: Vec3i) -> bool {
        match self.requests.as_ref() {
            Some(tx) => tx.send(LoadRequest::Load(coord)).is_ok(),
            None => false,
        }
    }

    /// Send a [`LoadRequest::Save`]. Failures are silently ignored — saves
    /// are fire-and-forget.
    pub fn request_save(&self, coord: Vec3i, bytes: Vec<u8>) {
        if let Some(tx) = self.requests.as_ref() {
            let _ = tx.send(LoadRequest::Save(coord, bytes));
        }
    }

    /// Drain every available [`LoadResult`] without blocking.
    #[must_use]
    pub fn drain_results(&self) -> Vec<LoadResult> {
        let mut out = Vec::new();
        while let Ok(r) = self.results.try_recv() {
            out.push(r);
        }
        out
    }
}

impl Drop for ChunkPipeline {
    fn drop(&mut self) {
        // Closing the request channel signals the worker to exit on its next
        // `recv`. Order matters: drop the sender, *then* join.
        self.requests.take();
        if let Some(h) = self.handle.take() {
            let _ = h.join();
        }
    }
}

/// Worker entry point. Owns its channel ends + worldgen state for the
/// thread's whole lifetime (drop-on-exit closes the channels). Processes
/// requests until the request channel disconnects.
#[allow(clippy::needless_pass_by_value, clippy::too_many_arguments)] // values are dropped on thread exit
fn worker_loop(
    requests: Receiver<LoadRequest>,
    results: Sender<LoadResult>,
    _registry: Arc<BlockRegistry>,
    base_blocks: BaseBlocks,
    generator: Generator,
    db: Arc<sled::Db>,
    height_map_size: usize,
    canonical_to_current: Arc<Vec<BlockId>>,
) {
    // `_registry` isn't consulted by terrain gen today (which goes through
    // `BaseBlocks` ids); the Arc is held for the worker's lifetime so future
    // worldgen extensions can reach for it without re-plumbing.
    let mut height_map = HeightMap::new(height_map_size);
    while let Ok(req) = requests.recv() {
        match req {
            LoadRequest::Load(coord) => {
                let chunk = build_chunk(
                    coord,
                    &mut height_map,
                    &generator,
                    &base_blocks,
                    &db,
                    &canonical_to_current,
                );
                if results.send(LoadResult { coord, chunk }).is_err() {
                    break;
                }
            }
            LoadRequest::Save(coord, bytes) => {
                if let Err(err) = db.insert(chunk_key(coord), bytes) {
                    tracing::warn!(?coord, error = %err, "chunk pipeline save failed");
                }
            }
        }
    }
}

/// Build a chunk for `coord`: load bytes from sled if present, otherwise
/// generate fresh terrain. Always runs `post_init` before returning.
fn build_chunk(
    coord: Vec3i,
    height_map: &mut HeightMap,
    generator: &Generator,
    base_blocks: &BaseBlocks,
    db: &sled::Db,
    canonical_to_current: &[BlockId],
) -> Chunk {
    let mut chunk = Chunk::new(coord);
    let from_disk = match db.get(chunk_key(coord)) {
        Ok(Some(ivec)) => match chunk.unpackage_from(ivec.as_ref(), canonical_to_current) {
            Ok(()) => true,
            Err(err) => {
                tracing::warn!(?coord, error = %err, "chunk unpackage failed; regenerating");
                false
            }
        },
        Ok(None) => false,
        Err(err) => {
            tracing::warn!(?coord, error = %err, "sled read failed; regenerating");
            false
        }
    };
    if !from_disk {
        chunk.init_generate(height_map, generator, base_blocks);
    }
    chunk
}

/// Sled key for a chunk coord. Mirrors the layout in `super::store::TilesStore`
/// — the worker writes directly to sled, so it must use the same key shape.
fn chunk_key(ccoord: Vec3i) -> [u8; 12] {
    let mut k = [0u8; 12];
    k[0..4].copy_from_slice(&ccoord.x.to_le_bytes());
    k[4..8].copy_from_slice(&ccoord.y.to_le_bytes());
    k[8..12].copy_from_slice(&ccoord.z.to_le_bytes());
    k
}

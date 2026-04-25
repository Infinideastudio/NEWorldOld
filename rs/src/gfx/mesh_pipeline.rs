//! Async chunk meshing pipeline ([F6] in `docs/rust_migration.md` §5).
//!
//! Spawns one background worker thread that owns a clone of
//! `Arc<BlockRegistry>`. The main thread builds a [`MeshInput`] from the
//! world's 27-chunk neighborhood and ships it through a `crossbeam-channel`;
//! the worker runs [`mesh_chunk`] and returns a [`MeshOutput`] tagged with
//! the original coord. The main thread then re-resolves the coord through
//! `World::chunk_by_coord` (§2.5) and uploads the result via
//! `ChunkMesh::upload`.
//!
//! ## Why a single worker
//!
//! `mesh_chunk` is pure CPU work; one worker is enough to overlap meshing
//! with the main render thread for the small render distances we ship today.
//! Future scaling would replace this with a `rayon` pool, but the channel
//! shape stays the same.

use std::sync::Arc;
use std::thread::JoinHandle;

use crossbeam_channel::{Receiver, Sender, unbounded};

use crate::blocks::BlockRegistry;
use crate::gfx::mesh::{MeshInput, MeshOutput, mesh_chunk};

/// A meshing job. The main thread owns the `MeshInput` (an 18×18×18 padded
/// `BlockData` snapshot) and ships it to the worker via the request channel.
pub struct MeshRequest {
    pub input: MeshInput,
}

/// Result of a meshing job. The `MeshOutput` is tagged with the original
/// coord so the main thread can re-resolve the slot through `by_coord`
/// (§2.5) rather than reusing a possibly-stale `ChunkKey`.
pub struct MeshDone {
    pub output: MeshOutput,
}

/// Owner-side handle for the mesh worker. Drops the request channel on
/// `drop`, signalling the worker to exit; joins the thread.
pub struct MeshPipeline {
    requests: Option<Sender<MeshRequest>>,
    results: Receiver<MeshDone>,
    handle: Option<JoinHandle<()>>,
}

impl MeshPipeline {
    /// Spawn the mesh worker thread.
    #[must_use]
    pub fn spawn(registry: Arc<BlockRegistry>) -> Self {
        let (req_tx, req_rx) = unbounded::<MeshRequest>();
        let (res_tx, res_rx) = unbounded::<MeshDone>();
        let handle = std::thread::Builder::new()
            .name("neworld-mesh-pipeline".into())
            .spawn(move || worker_loop(req_rx, res_tx, registry))
            .expect("spawn mesh pipeline worker");
        Self {
            requests: Some(req_tx),
            results: res_rx,
            handle: Some(handle),
        }
    }

    /// Submit a meshing job. Returns `false` if the worker channel is closed.
    #[must_use]
    pub fn submit(&self, input: MeshInput) -> bool {
        match self.requests.as_ref() {
            Some(tx) => tx.send(MeshRequest { input }).is_ok(),
            None => false,
        }
    }

    /// Drain every available [`MeshDone`] without blocking.
    #[must_use]
    pub fn drain(&self) -> Vec<MeshDone> {
        let mut out = Vec::new();
        while let Ok(d) = self.results.try_recv() {
            out.push(d);
        }
        out
    }
}

impl Drop for MeshPipeline {
    fn drop(&mut self) {
        self.requests.take();
        if let Some(h) = self.handle.take() {
            let _ = h.join();
        }
    }
}

#[allow(clippy::needless_pass_by_value)] // values are dropped on thread exit
fn worker_loop(
    requests: Receiver<MeshRequest>,
    results: Sender<MeshDone>,
    registry: Arc<BlockRegistry>,
) {
    while let Ok(req) = requests.recv() {
        let output = mesh_chunk(&req.input, &registry);
        if results.send(MeshDone { output }).is_err() {
            break;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::blocks::{BlockData, register_base_blocks};
    use crate::gfx::mesh::{PADDED_VOLUME, padded_index};
    use cgmath::Vector3;

    /// Build a `MeshInput` that places one stone block at the chunk-local
    /// center and air everywhere else (matches the `single_solid_block`
    /// fixture in `gfx::mesh`'s test suite).
    fn single_block_input() -> (MeshInput, Arc<BlockRegistry>) {
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        let registry = Arc::new(r);
        let buf: Box<[BlockData; PADDED_VOLUME]> =
            vec![BlockData::default(); PADDED_VOLUME]
                .into_boxed_slice()
                .try_into()
                .expect("PADDED_VOLUME-sized vec");
        let mut padded = buf;
        padded[padded_index(9, 9, 9)] = BlockData {
            id: base.stone,
            ..BlockData::default()
        };
        let input = MeshInput {
            coord: Vector3::new(0, 0, 0),
            padded,
        };
        (input, registry)
    }

    #[test]
    fn worker_meshes_known_input_correctly() {
        let (input, registry) = single_block_input();
        let pipeline = MeshPipeline::spawn(registry);
        assert!(pipeline.submit(input));
        // Wait up to ~5 s for the worker to spin up + finish. The mesh job
        // itself is microseconds, but Windows thread spawn latency can spike.
        let deadline = std::time::Instant::now() + std::time::Duration::from_secs(5);
        let mut done: Option<MeshDone> = None;
        while std::time::Instant::now() < deadline {
            let mut drained = pipeline.drain();
            if let Some(d) = drained.pop() {
                done = Some(d);
                break;
            }
            std::thread::sleep(std::time::Duration::from_millis(5));
        }
        let done = done.expect("mesh worker returned a result");
        // Reference: `gfx::mesh::tests::single_solid_block_emits_six_faces`
        // expects 6 faces × 6 verts/face = 36 opaque verts.
        assert_eq!(done.output.opaque.len(), 36);
        assert!(done.output.translucent.is_empty());
        assert_eq!(done.output.coord, Vector3::new(0, 0, 0));
    }
}

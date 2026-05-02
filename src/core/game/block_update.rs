//! Block writes + light-relaxation queue.
//!
//! Port of the gameplay write/tick functions from C++
//! `worlds/worlds.ixx` (`put_block`, `update_block`,
//! `process_block_updates`). The light scheme is a per-cell
//! relaxation: each call recomputes one cell's `(sky, block)` light
//! from the max of its 6 face neighbours minus 1 (with skylit and
//! light-source overrides), and queues the 6 neighbours for follow-up
//! whenever the cell's light actually changed. With a long-enough
//! drain budget the queue converges to the correct steady state,
//! using only local information per step.
//!
//! All entry points take `&World` (DashMap + per-chunk locks supply
//! the synchronisation) plus the `BaseBlocks` / `BlockRegistry` they
//! need to identify air, light sources, and solidity.
//!
//! `update_block` opens one [`WriteTxn`] per call covering the cell
//! and its 6 face neighbours (1–7 chunks, depending on whether the
//! cell sits at chunk edges). If any of those chunks isn't loaded the
//! call bails — the C++ behaviour. Light propagation across an
//! unloaded boundary just stops; the queue drops the work item
//! silently.

use std::collections::{HashMap, VecDeque};

use crate::core::blocks::{BaseBlocks, BlockData, BlockId, BlockLight, BlockRegistry, BlockState};
use crate::core::math::Vec3i;
use crate::core::world::{WorkingSet, World, WriteTxn, chunk_coord};

// ----------------------------------------------------------------------
//   Tuning constants
// ----------------------------------------------------------------------

/// Maximum entries drained from the queue per tick.
pub const MAX_BLOCK_UPDATES: usize = 65536;
/// Random ticks fired per loaded chunk per random-tick pass.
pub const RANDOM_UPDATES_PER_CHUNK: usize = 3;
/// Random-tick pass period in sim ticks (one pass every N ticks).
pub const RANDOM_UPDATE_PERIOD: u32 = 30;

/// Maximum value of either light channel.
const LIGHT_MAX: u8 = 15;

/// 6 face-neighbour offsets in the order the C++ code uses
/// (`+x, -x, +y, -y, +z, -z`). Order matters: index 2 is the **top**
/// neighbour, which the skylit shortcut consults.
const NEIGHBOUR_OFFSETS: [Vec3i; 6] = [
    Vec3i {
        x: 1,
        y: 0,
        z: 0,
    },
    Vec3i {
        x: -1,
        y: 0,
        z: 0,
    },
    Vec3i {
        x: 0,
        y: 1,
        z: 0,
    },
    Vec3i {
        x: 0,
        y: -1,
        z: 0,
    },
    Vec3i {
        x: 0,
        y: 0,
        z: 1,
    },
    Vec3i {
        x: 0,
        y: 0,
        z: -1,
    },
];

// ----------------------------------------------------------------------
//   BlockUpdateQueue
// ----------------------------------------------------------------------

/// FIFO of block coords whose neighbourhood needs an update pass. The
/// relaxation step ([`process_block_updates`]) drains this each tick.
#[derive(Default)]
pub struct BlockUpdateQueue {
    queue: VecDeque<Vec3i>,
}

impl BlockUpdateQueue {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn len(&self) -> usize {
        self.queue.len()
    }

    pub fn is_empty(&self) -> bool {
        self.queue.is_empty()
    }

    /// Borrow the underlying queue. Used by HUD / stats readers.
    pub fn as_deque(&self) -> &VecDeque<Vec3i> {
        &self.queue
    }

    /// Append one coord to the back of the queue.
    pub fn push(&mut self, coord: Vec3i) {
        self.queue.push_back(coord);
    }

    /// Pop the front coord, or `None` if empty.
    pub fn pop_front(&mut self) -> Option<Vec3i> {
        self.queue.pop_front()
    }
}

// ----------------------------------------------------------------------
//   Block writes
// ----------------------------------------------------------------------

/// Write a block id at `coord`, preserving the existing light/state.
/// If `queue_update`, also fires a fresh [`update_block`] on the cell
/// so light propagates outward.
pub fn set_block(
    world: &World,
    queue: &mut BlockUpdateQueue,
    base: &BaseBlocks,
    registry: &BlockRegistry,
    coord: Vec3i,
    id: BlockId,
    queue_update: bool,
) {
    set_id_and_state(world, coord, Some(id), None);
    if queue_update {
        update_block(world, queue, base, registry, coord, true);
    }
}

/// `set_block` variant that also writes a `BlockState` byte. Used by
/// orientation-bearing placements (logs etc.).
#[allow(clippy::too_many_arguments)] // mirrors the C++ `put_block` shape
pub fn set_block_with_state(
    world: &World,
    queue: &mut BlockUpdateQueue,
    base: &BaseBlocks,
    registry: &BlockRegistry,
    coord: Vec3i,
    id: BlockId,
    state: BlockState,
    queue_update: bool,
) {
    set_id_and_state(world, coord, Some(id), Some(state));
    if queue_update {
        update_block(world, queue, base, registry, coord, true);
    }
}

/// Apply one cell mutation under a 1-coord `WriteTxn`. `id_opt` /
/// `state_opt` mean "leave the existing field alone" when `None`.
/// Always preserves the existing light — `update_block` is the only
/// thing that writes light. After commit, marks the cell's chunk plus
/// its 26 neighbours as needing a re-mesh (handles chunk-edge cases
/// without dedicated branches).
fn set_id_and_state(
    world: &World,
    coord: Vec3i,
    id_opt: Option<BlockId>,
    state_opt: Option<BlockState>,
) {
    let ccoord = chunk_coord(coord);
    if !world.is_loaded(ccoord) {
        return;
    }
    let Ok(mut txn) = world.begin_write_txn_sync(WorkingSet::Single(ccoord)) else {
        return;
    };
    let Ok(curr) = txn.read(coord) else {
        return;
    };
    let new_data = BlockData {
        id: id_opt.unwrap_or(curr.id),
        state: state_opt.unwrap_or(curr.state),
        light: curr.light,
    };
    if new_data == curr {
        return;
    }
    if txn.write(coord, new_data).is_err() {
        return;
    }
    txn.commit();
    world.mark_neighbour_chunks_updated(ccoord);
}

// ----------------------------------------------------------------------
//   Light relaxation
// ----------------------------------------------------------------------

/// Run one block-update step on `coord`: recompute its `(sky, block)`
/// light from the 6 face neighbours, write back if different, and
/// queue the 6 neighbours so propagation continues.
///
/// Returns `false` if the cell or any of its 6 neighbour-chunks isn't
/// loaded — the same skip-on-missing behaviour as the C++ original
/// (the scheme is monotonic, so dropping the work item is safe).
///
/// `initial = true` forces the neighbour queueing even when the cell's
/// own light didn't change. Callers from [`set_block`] pass `true` so
/// the surrounding light field gets a chance to relax against the
/// just-written block.
///
/// Single-coord entry point: opens its own 7-chunk `WriteTxn`. The
/// per-tick [`process_block_updates`] driver bypasses this and batches
/// many coords through one txn — see that function for the rationale.
pub fn update_block(
    world: &World,
    queue: &mut BlockUpdateQueue,
    base: &BaseBlocks,
    registry: &BlockRegistry,
    coord: Vec3i,
    initial: bool,
) -> bool {
    let ccoord = chunk_coord(coord);
    let chunks = chunk_neighborhood(ccoord);
    for &cc in &chunks {
        if !world.is_loaded(cc) {
            return false;
        }
    }
    let Ok(mut txn) = world.begin_write_txn_sync(WorkingSet::List(chunks)) else {
        return false;
    };
    let changed = process_one_in_txn(&mut txn, queue, base, registry, coord, initial);
    txn.commit();
    if changed {
        world.mark_neighbour_chunks_updated(ccoord);
    }
    true
}

/// The 7 chunk coords a single-cell update needs locked: the cell's
/// own chunk plus its 6 face-neighbour chunks. All cells inside one
/// chunk share this exact set, which is why batching by cell-chunk
/// works.
fn chunk_neighborhood(ccoord: Vec3i) -> Vec<Vec3i> {
    let mut v = Vec::with_capacity(7);
    v.push(ccoord);
    for off in NEIGHBOUR_OFFSETS {
        v.push(ccoord + off);
    }
    v
}

/// Per-cell update body: assumes the caller already holds a `WriteTxn`
/// covering the 7-chunk neighbourhood of `coord`. Returns `true` if
/// the cell's `BlockData` actually changed (the caller uses this to
/// drive `mark_neighbour_chunks_updated` — a single mark-call per
/// chunk-batch suffices).
fn process_one_in_txn(
    txn: &mut WriteTxn,
    queue: &mut BlockUpdateQueue,
    base: &BaseBlocks,
    registry: &BlockRegistry,
    coord: Vec3i,
    initial: bool,
) -> bool {
    let neighbour_coords: [Vec3i; 6] = NEIGHBOUR_OFFSETS.map(|o| coord + o);

    let Ok(curr) = txn.read(coord) else {
        return false;
    };
    let mut neighbours = [BlockData::default(); 6];
    for (i, nc) in neighbour_coords.iter().enumerate() {
        match txn.read(*nc) {
            Ok(b) => neighbours[i] = b,
            Err(_) => return false,
        }
    }

    let (sky_light, block_light) = compute_light(curr, &neighbours, base, registry, coord);
    let new_data = BlockData {
        id: curr.id,
        state: curr.state,
        light: BlockLight::new(sky_light, block_light),
    };

    let mut changed = false;
    if new_data != curr && txn.write(coord, new_data).is_ok() {
        changed = true;
    }
    if changed || initial {
        for nc in neighbour_coords {
            queue.push(nc);
        }
    }
    changed
}

/// Per-cell light recomputation, factored out for clarity. Returns
/// `(sky, block)` channels in `0..=15`.
///
/// Mirrors C++ `worlds.ixx::update_block` (lines 440–470):
///
/// * Air: `max(neighbour) - 1` (or `LIGHT_MAX` if directly under sky).
/// * Non-solid (water, leaf, glass): `max(neighbour) - 1`.
/// * Solid: `0` for both channels.
/// * Glowstone / lava: block-light forced to `LIGHT_MAX` regardless.
///
/// The "skylit shortcut" (`y >= 0` and the top neighbour at full sky
/// light) lets a vertical column of air keep `sky == 15` without the
/// usual −1 attenuation per step — vastly fewer iterations to reach
/// the steady state in open daylight.
fn compute_light(
    curr: BlockData,
    neighbours: &[BlockData; 6],
    base: &BaseBlocks,
    registry: &BlockRegistry,
    coord: Vec3i,
) -> (u8, u8) {
    let mut sky = 0u8;
    let mut block = 0u8;
    for n in neighbours {
        sky = sky.max(n.light.sky());
        block = block.max(n.light.block());
    }
    let skylit = coord.y >= 0 && neighbours[2].light.sky() == LIGHT_MAX;

    if curr.id == base.air {
        sky = if skylit { LIGHT_MAX } else { sky.saturating_sub(1) };
        block = block.saturating_sub(1);
    } else if !registry.get(curr.id).solid {
        sky = sky.saturating_sub(1);
        block = block.saturating_sub(1);
    } else {
        sky = 0;
        block = 0;
    }

    if curr.id == base.glowstone || curr.id == base.lava {
        block = LIGHT_MAX;
    }
    (sky, block)
}

// ----------------------------------------------------------------------
//   Tick driver
// ----------------------------------------------------------------------

/// Drain up to [`MAX_BLOCK_UPDATES`] coords from the queue per call,
/// running the per-cell update on each. Called once per sim tick.
///
/// **Batching.** Coords are grouped by cell-chunk before processing.
/// Every cell in one chunk needs the same 7-chunk working set
/// (cell-chunk + 6 face neighbours), so one `WriteTxn` covers the
/// entire group — we pay the lock-acquire cost once per chunk-batch
/// instead of once per cell. With a flat queue of 65536 entries
/// spread across, say, 50 chunks, this is 50 txns instead of 65536.
///
/// Cross-chunk neighbours that get queued during a batch land back
/// in the main queue and are picked up on the next outer iteration —
/// they can't go into the *current* batch because their working set
/// doesn't fit inside the held one. Snapshot semantics match the
/// per-cell version: queue order within a chunk-group is preserved,
/// and `WriteTxn::read` honours buffered writes from earlier cells in
/// the same group.
pub fn process_block_updates(
    world: &World,
    queue: &mut BlockUpdateQueue,
    base: &BaseBlocks,
    registry: &BlockRegistry,
) {
    let mut budget = MAX_BLOCK_UPDATES;
    while budget > 0 && !queue.is_empty() {
        let take = budget.min(queue.len());
        let mut by_chunk: HashMap<Vec3i, Vec<Vec3i>> = HashMap::new();
        for _ in 0..take {
            let coord = queue
                .pop_front()
                .expect("queue is non-empty per the loop guard");
            by_chunk
                .entry(chunk_coord(coord))
                .or_default()
                .push(coord);
        }
        budget -= take;

        for (ccoord, coords) in by_chunk {
            let chunks = chunk_neighborhood(ccoord);
            if chunks.iter().any(|c| !world.is_loaded(*c)) {
                continue;
            }
            let Ok(mut txn) = world.begin_write_txn_sync(WorkingSet::List(chunks)) else {
                continue;
            };
            let mut any_changed = false;
            for coord in coords {
                if process_one_in_txn(&mut txn, queue, base, registry, coord, false) {
                    any_changed = true;
                }
            }
            txn.commit();
            if any_changed {
                world.mark_neighbour_chunks_updated(ccoord);
            }
        }
    }
}

/// Stub — fire one random-tick pass over every loaded chunk. Throttled
/// internally to [`RANDOM_UPDATE_PERIOD`]. No tickable blocks defined
/// yet, so nothing to do.
pub fn random_update(
    _world: &World,
    _queue: &mut BlockUpdateQueue,
    _base: &BaseBlocks,
    _registry: &BlockRegistry,
) {
}

/// Stub — explode a sphere of radius `radius` around `center`. Used by
/// `/explode` and TNT detonation. Not ported in this round.
pub fn explode(
    _world: &World,
    _queue: &mut BlockUpdateQueue,
    _base: &BaseBlocks,
    _registry: &BlockRegistry,
    _center: Vec3i,
    _radius: i32,
) {
}

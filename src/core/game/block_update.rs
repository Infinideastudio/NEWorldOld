//! `BlockUpdateQueue` + the gameplay write/tick functions that
//! consume it.
//!
//! These all take `&mut World` and live outside `core::world` because
//! they are gameplay mechanics — the database fragment doesn't know
//! about block-update relaxation, random ticks, tree generation, or
//! explosions. They are stubs in this round (the chunk-store
//! migration disconnected gameplay writes); the public surface is
//! preserved so `commands/base.rs` and `Game::tick_sim` keep
//! compiling.

use std::collections::VecDeque;

use crate::blocks::{Id, State};
use crate::core::world::World;
use crate::math::Vec3i;

// ----------------------------------------------------------------------
//   Tuning constants
// ----------------------------------------------------------------------

/// Maximum entries drained from the queue per tick.
pub const MAX_BLOCK_UPDATES: usize = 65536;
/// Random ticks fired per loaded chunk per random-tick pass.
pub const RANDOM_TICKS_PER_CHUNK: usize = 3;
/// Random-tick pass period in sim ticks (one pass every N ticks).
pub const RANDOM_TICK_PERIOD: u32 = 30;

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
}

/// Stub — write a block id at `coord`. Reactivating: open a 1-coord
/// `WriteTxn`, write, commit; mark the 27-cube updated; queue
/// neighbours for the relaxation pass.
pub fn set_block(
    _world: &mut World,
    _queue: &mut BlockUpdateQueue,
    _coord: Vec3i,
    _id: Id,
    _queue_update: bool,
) {
}

/// Stub — see [`set_block`]. Variant that also writes a state byte.
pub fn set_block_with_state(
    _world: &mut World,
    _queue: &mut BlockUpdateQueue,
    _coord: Vec3i,
    _id: Id,
    _state: State,
    _queue_update: bool,
) {
}

/// Stub — run one block-update relaxation step on `coord`. Returns
/// `true` if the cell was processed (all neighbours loaded).
/// Reactivating: 7-coord `WriteTxn` (cell + 6 face neighbours), light
/// recompute, queue further neighbours if anything changed.
pub fn update_block(
    _world: &mut World,
    _queue: &mut BlockUpdateQueue,
    _coord: Vec3i,
    _initial: bool,
) -> bool {
    false
}

/// Stub — drain up to [`MAX_BLOCK_UPDATES`] blocks from the queue and
/// run [`update_block`] on each.
pub fn process_block_updates(_world: &mut World, _queue: &mut BlockUpdateQueue) {}

/// Stub — fire one random-tick pass over every loaded chunk. Throttled
/// internally to [`RANDOM_TICK_PERIOD`].
pub fn random_tick(_world: &mut World, _queue: &mut BlockUpdateQueue) {}

/// Stub — plant a tree at `coord`. Used by `/build_tree`.
pub fn build_tree(_world: &mut World, _coord: Vec3i) {}

/// Stub — explode a sphere of radius `radius` around `center`. Used by
/// `/explode`.
pub fn explode(_world: &mut World, _center: Vec3i, _radius: i32) {}

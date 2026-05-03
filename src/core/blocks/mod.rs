//! Block primitives and registries (server-safe).
//!
//! Each submodule owns one concept: the data types that travel through
//! chunks (`BlockId`, `BlockState`, `BlockLight`, `BlockData`), the
//! per-block static info and registry, and the orientation/face helpers
//! used for placement and (on the client) the chunk mesher. Everything is
//! dynamically registered — no module-level globals — so a mod can
//! introduce new blocks by registering them at boot.
//!
//! `BlockId(0)` (a.k.a. [`BlockId::EMPTY`]) is the registry's reserved
//! empty slot. `BlockData::default()` is all-zero memory and references
//! that slot, so chunk arrays initialised by
//! `[BlockData::default(); SIZE_CUBED]` land on a valid "nothing here"
//! cell without any per-cell setup. `register_base_blocks` leaves the
//! empty slot intact and exposes `BaseBlocks.air = BlockId::EMPTY` for
//! callers that want to ask about "air" by name.
//!
//! Texture indices, per-block face textures, and the block-render
//! registry live in `client::blocks` — none of that is needed by a
//! headless server.

mod data;
mod face;
mod info;
mod orientation;
mod registry;

pub use data::{BlockData, BlockId, BlockLight, BlockState};
pub use face::{BlockFaceMapping, face_axis, face_index_static};
pub use info::BlockInfo;
pub use orientation::BlockOrientation;
pub use registry::BlockRegistry;

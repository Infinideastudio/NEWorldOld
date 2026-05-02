//! `core::world` — server-safe world primitives.
//!
//! Hosts the chunk store: `Chunk` is the cache slot (lock-protected
//! block array + paging atomics) and `World` is the database that
//! owns the `DashMap<Vec3i, Arc<Chunk>>` plus the transactional
//! read/write API.

pub mod chunk;
mod error;
pub mod metadata;
mod store;
mod txn;
#[allow(clippy::module_inception)]
mod world;

pub use chunk::{Blocks, Chunk};
pub use error::WorldError;
pub use metadata::Metadata;
pub use store::Store;
pub use txn::{ReadTxn, TxnError, WorkingSet, WriteTxn};
pub use world::*;

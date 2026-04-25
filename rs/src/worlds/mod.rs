//! `worlds` — World + Player, mirroring the C++ `src/worlds/` package.
//!
//! Re-exports the public surface of the two submodules so callers can write
//! `crate::worlds::World` / `crate::worlds::Player` (matching C++
//! `worlds::World` / `player::Player` namespacing).

pub mod player;
pub mod world;

pub use self::player::{GameMode, Player, PlayerError};
pub use self::world::{
    BlockView, ChunkGrid, ChunkKey, MAX_BLOCK_UPDATES, MAX_CHUNK_LOADS, MAX_CHUNK_UNLOADS,
    TilesStore, World, WorldError, block_coord, chunk_coord,
};

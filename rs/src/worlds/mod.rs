//! `worlds` — direct mirror of `src/worlds/` in the C++ tree.
//!
//! | C++ file                              | Rust module                     |
//! |---------------------------------------|---------------------------------|
//! | `worlds.ixx`                          | [`world`]                       |
//! | `player.ixx` + `player_impl.cpp`      | [`player`]                      |
//! | `chunk_rendering.cpp`                 | [`chunk_rendering`]             |
//! | `forward.ixx`                         | (not needed — Rust uses paths)  |
//! | `world_rendering.cpp` (small)         | folded into `render` for now    |

pub mod chunk_rendering;
pub mod player;
pub mod world;

pub use self::player::{GameMode, Player, PlayerError};
pub use self::world::{
    BlockView, MAX_BLOCK_UPDATES, MAX_CHUNK_LOADS, MAX_CHUNK_UNLOADS, TilesStore, World,
    WorldError, block_coord, chunk_coord,
};

//! `worlds` — direct mirror of `src/worlds/` in the C++ tree.
//!
//! | C++ file                              | Rust module                     |
//! |---------------------------------------|---------------------------------|
//! | `worlds.ixx`                          | [`world`]                       |
//! | `player.ixx` + `player_impl.cpp`      | [`player`]                      |
//! | `chunk_rendering.cpp`                 | [`chunk_rendering`]             |

pub mod chunk_rendering;
pub mod player;
pub mod world;

pub use self::player::{GameMode, Player, PlayerError};
pub use self::world::{
    ReadTxn, Store, TxnError, WorkingSet, World, WorldError, WriteTxn, block_coord, chunk_coord,
};

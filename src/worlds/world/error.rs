//! `WorldError` — top-level error type for the world subsystem.

use thiserror::Error;

use crate::chunks::ChunkError;
use crate::worlds::player::PlayerError;

#[derive(Debug, Error)]
pub enum WorldError {
    #[error("world I/O failure: {0}")]
    Io(#[from] std::io::Error),
    #[error("sled DB failure: {0}")]
    Sled(#[from] sled::Error),
    #[error("chunk codec failure: {0}")]
    Chunk(#[from] ChunkError),
    #[error("player save/load failure: {0}")]
    Player(#[from] PlayerError),
}

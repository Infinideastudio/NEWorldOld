//! `WorldError` — top-level error type for the world subsystem.

use std::path::PathBuf;

use thiserror::Error;

#[derive(Debug, Error)]
pub enum MetadataError {
    #[error("world metadata I/O error at {path}: {source}")]
    Io {
        path: PathBuf,
        #[source]
        source: std::io::Error,
    },
    #[error("world metadata: bad magic (file is not a NEWorld world.dat)")]
    BadMagic,
    #[error("world metadata: unsupported version {got}")]
    BadVersion { got: u32 },
    #[error("world metadata: bincode encode error: {0}")]
    Encode(#[from] bincode::error::EncodeError),
    #[error("world metadata: bincode decode error: {0}")]
    Decode(#[from] bincode::error::DecodeError),
}

#[derive(Debug, Error, PartialEq, Eq)]
pub enum ChunkError {
    #[error("chunk header has bad magic")]
    Magic,
    #[error("chunk header has bad version: got {got}")]
    Version { got: u32 },
    #[error("chunk has bad size: expected {expected} bytes, got {got}")]
    Size { expected: usize, got: usize },
    #[error("chunk body failed zstd decompression")]
    Compression,
}

#[derive(Debug, Error)]
pub enum WorldError {
    #[error("world I/O failure: {0}")]
    Io(#[from] std::io::Error),
    #[error("sled DB failure: {0}")]
    Sled(#[from] sled::Error),
    #[error("chunk codec failure: {0}")]
    Chunk(#[from] ChunkError),
    #[error("world metadata failure: {0}")]
    Metadata(#[from] MetadataError),
}

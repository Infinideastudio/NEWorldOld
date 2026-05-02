//! Per-world block-id mapping table — saved to `<world_dir>/world.dat`.
//!
//! The mapping is a `Vec<String>` of internal block names: index `i` holds
//! the name of the block whose canonical id is `i` *in this world*.
//! Different worlds can have different mappings (different mod sets, plugin
//! load order, etc.); the in-memory `BlockRegistry` is a moving target, so
//! the world records its own canonical id space and the loader translates
//! at the chunk read/write boundary.
//!
//! On-disk format (mirrors the player save's magic+version preamble):
//!
//! ```text
//! +---------+---------+----------------------------+
//! | magic   | version | bincode-encoded body       |
//! | u32 LE  | u32 LE  | (variable)                 |
//! +---------+---------+----------------------------+
//! ```
//!
//! `magic = "NEWD"` and `version = 1`. Loads fail with [`MetadataError`] when
//! either field doesn't match — there is no migration path for an
//! unrecognised version.

use std::fs;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};

use bincode::config::Configuration;
use serde::{Deserialize, Serialize};
use thiserror::Error;

use crate::core::blocks::{BlockId, BlockRegistry};

/// Magic bytes for the world metadata file (`b"NEWD"` little-endian).
pub const WORLD_META_MAGIC: u32 = 0x4E45_5744;
/// Current world metadata format version.
pub const WORLD_META_VERSION: u32 = 1;
/// Size of the magic + version preamble.
const HEADER_SIZE: usize = 8;

#[derive(Debug, Error)]
pub enum MetadataError {
    /// I/O failure reading or writing the metadata file.
    #[error("world metadata I/O error at {path}: {source}")]
    Io {
        path: PathBuf,
        #[source]
        source: std::io::Error,
    },
    /// File does not start with the expected `b"NEWD"` magic.
    #[error("world metadata: bad magic (file is not a NEWorld world.dat)")]
    BadMagic,
    /// Save format version is not recognised.
    #[error("world metadata: unsupported version {got}")]
    BadVersion { got: u32 },
    /// Bincode failed to encode the metadata body.
    #[error("world metadata: bincode encode error: {0}")]
    Encode(#[from] bincode::error::EncodeError),
    /// Bincode failed to decode the metadata body.
    #[error("world metadata: bincode decode error: {0}")]
    Decode(#[from] bincode::error::DecodeError),
}

fn bincode_config() -> Configuration {
    bincode::config::standard()
}

/// Per-world canonical block-id mapping.
///
/// `block_mapping[i]` is the internal name (`info.name`) of the block whose
/// canonical id in this world is `i`. The empty slot at index 0 is always
/// `"empty"`.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct WorldMetadata {
    pub block_mapping: Vec<String>,
}

impl WorldMetadata {
    /// Snapshot the current `registry` into a canonical mapping. Each block
    /// id from `0..registry.len()` contributes its internal name.
    #[must_use]
    pub fn from_registry(registry: &BlockRegistry) -> Self {
        let mut block_mapping = Vec::with_capacity(registry.len());
        for entry in registry.entries() {
            block_mapping.push(entry.name.to_string());
        }
        Self { block_mapping }
    }

    /// Load metadata from `path`. Validates magic + version before decoding.
    pub fn load_from(path: &Path) -> Result<Self, MetadataError> {
        let mut file = fs::File::open(path).map_err(|source| MetadataError::Io {
            path: path.to_path_buf(),
            source,
        })?;
        let mut bytes = Vec::new();
        file.read_to_end(&mut bytes).map_err(|source| MetadataError::Io {
            path: path.to_path_buf(),
            source,
        })?;
        if bytes.len() < HEADER_SIZE {
            return Err(MetadataError::BadMagic);
        }
        let magic = u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
        if magic != WORLD_META_MAGIC {
            return Err(MetadataError::BadMagic);
        }
        let version = u32::from_le_bytes([bytes[4], bytes[5], bytes[6], bytes[7]]);
        if version != WORLD_META_VERSION {
            return Err(MetadataError::BadVersion { got: version });
        }
        let (meta, _used) = bincode::serde::decode_from_slice::<Self, _>(
            &bytes[HEADER_SIZE..],
            bincode_config(),
        )?;
        Ok(meta)
    }

    /// Save metadata atomically to `path` (write to `<path>.tmp`, then rename).
    pub fn save_to(&self, path: &Path) -> Result<(), MetadataError> {
        let mut buf = Vec::with_capacity(HEADER_SIZE + 256);
        buf.extend_from_slice(&WORLD_META_MAGIC.to_le_bytes());
        buf.extend_from_slice(&WORLD_META_VERSION.to_le_bytes());
        let body = bincode::serde::encode_to_vec(self, bincode_config())?;
        buf.extend_from_slice(&body);

        let tmp = path.with_extension("tmp");
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent).map_err(|source| MetadataError::Io {
                path: parent.to_path_buf(),
                source,
            })?;
        }
        {
            let mut file = fs::File::create(&tmp).map_err(|source| MetadataError::Io {
                path: tmp.clone(),
                source,
            })?;
            file.write_all(&buf).map_err(|source| MetadataError::Io {
                path: tmp.clone(),
                source,
            })?;
            file.sync_all().map_err(|source| MetadataError::Io {
                path: tmp.clone(),
                source,
            })?;
        }
        fs::rename(&tmp, path).map_err(|source| MetadataError::Io {
            path: path.to_path_buf(),
            source,
        })?;
        Ok(())
    }

    /// Build the load-side translation table: `canonical_id → current
    /// BlockId`. For each canonical name look it up in `registry`; missing
    /// names map to [`BlockId::EMPTY`] so chunks referencing an absent block
    /// (e.g. a mod was removed) load as empty rather than failing the
    /// whole world.
    #[must_use]
    pub fn canonical_to_current(&self, registry: &BlockRegistry) -> Vec<BlockId> {
        self.block_mapping
            .iter()
            .map(|name| {
                if name == "empty" {
                    // Slot 0 is always the empty entry; preserve that id
                    // verbatim regardless of whether the current registry
                    // exposes it under a different name.
                    BlockId::EMPTY
                } else {
                    registry.id_of(name).unwrap_or(BlockId::EMPTY)
                }
            })
            .collect()
    }

    /// Build the save-side translation table: `current_registry_id →
    /// canonical_id (u16)`. For each current block, find its name in the
    /// canonical mapping; current ids whose names aren't in the mapping
    /// are assigned `u16::MAX` (the chunk encoder writes them as
    /// `BlockId::EMPTY`).
    #[must_use]
    pub fn current_to_canonical(&self, registry: &BlockRegistry) -> Vec<u16> {
        // Build a name → canonical_id reverse lookup once.
        let mut by_name = std::collections::HashMap::new();
        for (i, name) in self.block_mapping.iter().enumerate() {
            by_name.insert(name.as_str(), i as u16);
        }
        registry
            .entries()
            .iter()
            .map(|info| by_name.get(info.name.as_ref()).copied().unwrap_or(u16::MAX))
            .collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::core::blocks::register_base_blocks;
    use std::sync::atomic::{AtomicU64, Ordering};

    fn scratch_path(tag: &str) -> PathBuf {
        static COUNTER: AtomicU64 = AtomicU64::new(0);
        let n = COUNTER.fetch_add(1, Ordering::Relaxed);
        let pid = std::process::id();
        std::env::temp_dir().join(format!("neworld-meta-{tag}-{pid}-{n}.dat"))
    }

    #[test]
    fn snapshot_then_translate_is_identity_for_same_registry() {
        let mut r = BlockRegistry::new();
        let _base = register_base_blocks(&mut r);
        let meta = WorldMetadata::from_registry(&r);
        // For each registry entry, canonical -> current should round-trip.
        let load = meta.canonical_to_current(&r);
        let save = meta.current_to_canonical(&r);
        for i in 0..r.len() {
            assert_eq!(load[i], BlockId(i as u16));
            assert_eq!(save[i], i as u16);
        }
    }

    #[test]
    fn missing_canonical_name_maps_to_empty() {
        let mut r = BlockRegistry::new();
        let _base = register_base_blocks(&mut r);
        let mut meta = WorldMetadata::from_registry(&r);
        // Pretend the world saved a block that no current mod provides.
        meta.block_mapping.push("ghost.removed_block".to_string());
        let load = meta.canonical_to_current(&r);
        assert_eq!(load.last().copied(), Some(BlockId::EMPTY));
    }

    #[test]
    fn save_load_round_trips_through_temp_file() {
        let mut r = BlockRegistry::new();
        let _base = register_base_blocks(&mut r);
        let original = WorldMetadata::from_registry(&r);
        let path = scratch_path("rt");
        original.save_to(&path).expect("save");
        let loaded = WorldMetadata::load_from(&path).expect("load");
        assert_eq!(original, loaded);
        let _ = fs::remove_file(&path);
    }

    #[test]
    fn load_rejects_bad_magic() {
        let path = scratch_path("badmagic");
        fs::write(&path, b"XXXX\x01\x00\x00\x00").expect("write");
        let err = WorldMetadata::load_from(&path).unwrap_err();
        assert!(matches!(err, MetadataError::BadMagic));
        let _ = fs::remove_file(&path);
    }

    #[test]
    fn load_rejects_bad_version() {
        let path = scratch_path("badver");
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&WORLD_META_MAGIC.to_le_bytes());
        bytes.extend_from_slice(&999u32.to_le_bytes());
        fs::write(&path, &bytes).expect("write");
        let err = WorldMetadata::load_from(&path).unwrap_err();
        assert!(matches!(err, MetadataError::BadVersion { got: 999 }));
        let _ = fs::remove_file(&path);
    }
}

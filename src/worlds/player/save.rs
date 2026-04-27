//! Player save / load. Bincode-encoded body preceded by a 4+4-byte
//! `magic + version` header. No backward compatibility with C++ saves.

use std::fs;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};

use bincode::config::Configuration;
use thiserror::Error;

use super::Player;

/// Magic bytes for the player save file (`b"NEWP"` little-endian).
const PLAYER_SAVE_MAGIC: u32 = 0x4E45_5750;
/// Current player save format version. Bumped to 2 when the health /
/// fall-damage / heal-speed fields were removed — pre-bump saves no
/// longer decode (no migration path; this codebase doesn't promise
/// save-format stability mid-development).
const PLAYER_SAVE_VERSION: u32 = 2;

/// Errors returned by [`Player::save_to`] / [`Player::load_from`].
#[derive(Debug, Error)]
pub enum PlayerError {
    /// I/O failure reading or writing the save file.
    #[error("player save I/O error at {path}: {source}")]
    Io {
        path: PathBuf,
        #[source]
        source: std::io::Error,
    },
    /// File does not start with the expected `b"NEWP"` magic.
    #[error("player save: bad magic (file is not a NEWorld player save)")]
    BadMagic,
    /// Save format version is not recognised.
    #[error("player save: unsupported version {got}")]
    BadVersion { got: u32 },
    /// Bincode failed to encode the player.
    #[error("player save: bincode encode error: {0}")]
    Encode(#[from] bincode::error::EncodeError),
    /// Bincode failed to decode the player.
    #[error("player save: bincode decode error: {0}")]
    Decode(#[from] bincode::error::DecodeError),
}

fn bincode_config() -> Configuration {
    bincode::config::standard()
}

impl Player {
    /// Serialise to `path` atomically. Writes to `<path>.tmp` first, then
    /// renames into place.
    pub fn save_to(&self, path: &Path) -> Result<(), PlayerError> {
        let mut buf = Vec::with_capacity(8 + 256);
        buf.extend_from_slice(&PLAYER_SAVE_MAGIC.to_le_bytes());
        buf.extend_from_slice(&PLAYER_SAVE_VERSION.to_le_bytes());
        let body = bincode::serde::encode_to_vec(self, bincode_config())?;
        buf.extend_from_slice(&body);

        let tmp = path.with_extension("tmp");
        {
            let mut file = fs::File::create(&tmp).map_err(|source| PlayerError::Io {
                path: tmp.clone(),
                source,
            })?;
            file.write_all(&buf).map_err(|source| PlayerError::Io {
                path: tmp.clone(),
                source,
            })?;
            file.sync_all().map_err(|source| PlayerError::Io {
                path: tmp.clone(),
                source,
            })?;
        }
        fs::rename(&tmp, path).map_err(|source| PlayerError::Io {
            path: path.to_path_buf(),
            source,
        })?;
        Ok(())
    }

    /// Load a player from `path`. Validates magic + version before decoding.
    pub fn load_from(path: &Path) -> Result<Self, PlayerError> {
        let mut file = fs::File::open(path).map_err(|source| PlayerError::Io {
            path: path.to_path_buf(),
            source,
        })?;
        let mut bytes = Vec::new();
        file.read_to_end(&mut bytes).map_err(|source| PlayerError::Io {
            path: path.to_path_buf(),
            source,
        })?;
        if bytes.len() < 8 {
            return Err(PlayerError::BadMagic);
        }
        let magic = u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
        if magic != PLAYER_SAVE_MAGIC {
            return Err(PlayerError::BadMagic);
        }
        let version = u32::from_le_bytes([bytes[4], bytes[5], bytes[6], bytes[7]]);
        if version != PLAYER_SAVE_VERSION {
            return Err(PlayerError::BadVersion { got: version });
        }
        let (player, _used) =
            bincode::serde::decode_from_slice::<Self, _>(&bytes[8..], bincode_config())?;
        Ok(player)
    }
}

#[cfg(test)]
#[allow(clippy::float_cmp)]
mod tests {
    use super::*;
    use crate::blocks::Id;
    use crate::items::ItemStack;
    use crate::math::{Eulerd, Vec3d};
    use crate::worlds::player::GameMode;
    use std::sync::atomic::{AtomicU64, Ordering};

    /// Minimal scratch directory under `std::env::temp_dir()` — same shape as
    /// the helper in `i18n::tests`. Each instance gets a unique subdir; `Drop`
    /// does best-effort cleanup.
    struct ScratchDir {
        path: PathBuf,
    }

    impl ScratchDir {
        fn new(tag: &str) -> Self {
            static COUNTER: AtomicU64 = AtomicU64::new(0);
            let n = COUNTER.fetch_add(1, Ordering::Relaxed);
            let pid = std::process::id();
            let path =
                std::env::temp_dir().join(format!("neworld-player-{tag}-{pid}-{n}"));
            let _ = fs::remove_dir_all(&path);
            fs::create_dir_all(&path).expect("create scratch dir");
            Self { path }
        }

        fn path(&self) -> &Path {
            &self.path
        }
    }

    impl Drop for ScratchDir {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.path);
        }
    }

    #[test]
    fn save_load_round_trip_through_temp_file() {
        let dir = ScratchDir::new("save-load");
        let path = dir.path().join("player.bin");

        let mut original = Player::default();
        original.set_coord(Vec3d::new(1.5, 130.0, -2.5));
        original.set_orientation(Eulerd::new(0.3, 0.4, 0.0));
        original.set_game_mode(GameMode::Creative);
        original.set_held_item_stack_index(4);
        *original.inventory_item_stack_mut(2, 3) = ItemStack::new(Id(7), 42);

        original.save_to(&path).expect("save");
        let loaded = Player::load_from(&path).expect("load");

        assert_eq!(loaded.coord(), original.coord());
        assert_eq!(loaded.orientation(), original.orientation());
        assert_eq!(loaded.game_mode(), original.game_mode());
        assert_eq!(loaded.held_item_stack_index(), original.held_item_stack_index());
        assert_eq!(loaded.inventory_item_stack(2, 3).id, Id(7));
        assert_eq!(loaded.inventory_item_stack(2, 3).count, 42);
        assert!(loaded.flying());
    }

    #[test]
    fn load_from_bad_magic_errors() {
        let dir = ScratchDir::new("bad-magic");
        let path = dir.path().join("player.bin");
        // Write 16 bytes of 0xAA — magic mismatch.
        fs::write(&path, vec![0xAA_u8; 16]).expect("write");
        let err = Player::load_from(&path).expect_err("must error");
        assert!(matches!(err, PlayerError::BadMagic), "got {err:?}");
    }

    #[test]
    fn load_from_bad_version_errors() {
        let dir = ScratchDir::new("bad-version");
        let path = dir.path().join("player.bin");
        let mut bytes = Vec::new();
        bytes.extend_from_slice(&PLAYER_SAVE_MAGIC.to_le_bytes());
        bytes.extend_from_slice(&999_u32.to_le_bytes());
        // Some garbage payload — won't be reached.
        bytes.extend_from_slice(&[0_u8; 32]);
        fs::write(&path, &bytes).expect("write");
        let err = Player::load_from(&path).expect_err("must error");
        assert!(
            matches!(err, PlayerError::BadVersion { got: 999 }),
            "got {err:?}"
        );
    }

    #[test]
    fn load_from_short_file_errors_with_bad_magic() {
        let dir = ScratchDir::new("short-file");
        let path = dir.path().join("player.bin");
        fs::write(&path, b"NE").expect("write");
        let err = Player::load_from(&path).expect_err("must error");
        assert!(matches!(err, PlayerError::BadMagic), "got {err:?}");
    }
}

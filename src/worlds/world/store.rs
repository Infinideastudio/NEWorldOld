//! `TilesStore` — sled-backed K/V store keyed by chunk coord.
//!
//! On-disk layout (per `docs/rust_migration.md` §4.6, "no backward
//! compatibility"): a sled database whose keys are the 12-byte
//! little-endian `[i32; 3]` of each chunk coord and whose values are the
//! raw bytes from [`crate::chunks::Chunk::package_to`]. There is no header
//! on the value — versioning lives one level up in the chunk codec.
//!
//! The path on disk is chosen by the caller via [`Self::open_at`]; the
//! convenience [`Self::open`] uses the legacy cwd-relative
//! `worlds/<name>/chunks.db` shape for callers that don't carry a base
//! path of their own.

use std::path::{Path, PathBuf};
use std::sync::Arc;

use crate::math::Vec3i;

use super::error::WorldError;

pub struct TilesStore {
    /// Wrapped in `Arc` so the async chunk pipeline worker can hold a clone
    /// of the underlying sled handle without borrowing through `&World`.
    db: Arc<sled::Db>,
}

impl TilesStore {
    /// Open the sled DB at `db_path`, creating parent directories as needed.
    /// This is the path-explicit constructor — tests + production callers
    /// that already know where to put the world should use this instead of
    /// [`Self::open`] so they don't depend on cwd.
    pub fn open_at(db_path: &Path) -> Result<Self, WorldError> {
        if let Some(parent) = db_path.parent() {
            std::fs::create_dir_all(parent)?;
        }
        let db = Arc::new(sled::open(db_path)?);
        Ok(Self { db })
    }

    /// Convenience wrapper that opens `worlds/<world_name>/chunks.db`
    /// relative to the current working directory. Equivalent to
    /// `Self::open_at(&Path::new("worlds").join(world_name).join("chunks.db"))`.
    pub fn open(world_name: &str) -> Result<Self, WorldError> {
        let path = PathBuf::from("worlds")
            .join(world_name)
            .join("chunks.db");
        Self::open_at(&path)
    }

    /// Cheap clone of the underlying sled DB handle for a worker thread.
    /// `sled::Db` is internally `Arc`-shared and `Send + Sync`; the explicit
    /// `Arc` here just makes the sharing visible at the call site.
    #[must_use]
    pub fn db_handle(&self) -> Arc<sled::Db> {
        Arc::clone(&self.db)
    }

    fn key(ccoord: Vec3i) -> [u8; 12] {
        let mut k = [0u8; 12];
        k[0..4].copy_from_slice(&ccoord.x.to_le_bytes());
        k[4..8].copy_from_slice(&ccoord.y.to_le_bytes());
        k[8..12].copy_from_slice(&ccoord.z.to_le_bytes());
        k
    }

    /// Returns the raw chunk bytes if present.
    pub fn load(&self, ccoord: Vec3i) -> Result<Option<Vec<u8>>, WorldError> {
        let value = self.db.get(Self::key(ccoord))?;
        Ok(value.map(|ivec| ivec.to_vec()))
    }

    /// Writes raw bytes for a chunk coord.
    pub fn save(&self, ccoord: Vec3i, data: &[u8]) -> Result<(), WorldError> {
        self.db.insert(Self::key(ccoord), data)?;
        Ok(())
    }

    /// Flushes the sled writebuffer to disk.
    pub fn flush(&self) -> Result<(), WorldError> {
        self.db.flush()?;
        Ok(())
    }
}

impl std::fmt::Debug for TilesStore {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("TilesStore").finish_non_exhaustive()
    }
}

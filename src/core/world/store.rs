//! [`Store`] — sled-backed K/V store keyed by chunk coord.
//!
//! Keys are the 12-byte little-endian `[i32; 3]` of each chunk coord;
//! values are the raw bytes from [`crate::core::world::Blocks::package_to`].
//! There is no header on the value — versioning lives one level up in
//! the chunk codec. Cloning a `Store` shares the underlying
//! `Arc<sled::Db>` (cheap), so worker threads can hold their own
//! handle without borrowing through `&World`.

use std::path::Path;
use std::sync::Arc;

use crate::core::math::Vec3i;

use super::errors::WorldError;

/// Sled-backed K/V store keyed by chunk coord. Cheap-clonable.
#[derive(Clone)]
pub struct Store {
    db: Arc<sled::Db>,
}

impl Store {
    /// Open the sled DB at `db_path`, creating parent directories as
    /// needed.
    pub fn open_at(db_path: &Path) -> Result<Self, WorldError> {
        if let Some(parent) = db_path.parent() {
            std::fs::create_dir_all(parent)?;
        }
        let db = Arc::new(sled::open(db_path)?);
        Ok(Self { db })
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

impl std::fmt::Debug for Store {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Store").finish_non_exhaustive()
    }
}

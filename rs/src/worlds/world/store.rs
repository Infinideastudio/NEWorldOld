//! `TilesStore` — sled-backed K/V store keyed by chunk coord.
//!
//! On-disk layout (per `docs/rust_migration.md` §4.6, "no backward
//! compatibility"): `worlds/<name>/chunks.db/` is a sled database. Each chunk
//! is keyed by the 12-byte little-endian `[i32; 3]` of its chunk coord and
//! the value is the raw bytes from [`crate::chunks::Chunk::package_to`].
//! There is no header on the value — versioning lives one level up in the
//! chunk codec.

use std::path::PathBuf;

use crate::math::Vec3i;

use super::error::WorldError;

pub struct TilesStore {
    db: sled::Db,
}

impl TilesStore {
    /// Open `worlds/<world_name>/chunks.db`, creating parent dirs as needed.
    pub fn open(world_name: &str) -> Result<Self, WorldError> {
        let path = Self::db_path(world_name);
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent)?;
        }
        let db = sled::open(&path)?;
        Ok(Self { db })
    }

    fn db_path(world_name: &str) -> PathBuf {
        PathBuf::from("worlds").join(world_name).join("chunks.db")
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

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Mutex;
    use std::sync::atomic::{AtomicU64, Ordering};

    /// `TilesStore::open` resolves the DB path relative to the current
    /// working directory; chdir-into-temp + a process-global mutex serialises
    /// concurrent tests so they don't trip over each other.
    static TEST_LOCK: Mutex<()> = Mutex::new(());

    struct ScratchDir {
        path: PathBuf,
        prev_cwd: PathBuf,
    }

    impl ScratchDir {
        fn new(tag: &str) -> Self {
            static COUNTER: AtomicU64 = AtomicU64::new(0);
            let n = COUNTER.fetch_add(1, Ordering::Relaxed);
            let pid = std::process::id();
            let path = std::env::temp_dir()
                .join(format!("neworld-tilesstore-{tag}-{pid}-{n}"));
            let _ = std::fs::remove_dir_all(&path);
            std::fs::create_dir_all(&path).expect("create scratch dir");
            let prev_cwd = std::env::current_dir().expect("cwd");
            std::env::set_current_dir(&path).expect("chdir");
            Self { path, prev_cwd }
        }
    }

    impl Drop for ScratchDir {
        fn drop(&mut self) {
            let _ = std::env::set_current_dir(&self.prev_cwd);
            let _ = std::fs::remove_dir_all(&self.path);
        }
    }

    #[test]
    fn round_trips_raw_bytes() {
        let _guard = TEST_LOCK.lock().unwrap();
        let _scratch = ScratchDir::new("round-trip");
        let store = TilesStore::open("test-world").expect("open");
        let coord = Vec3i::new(-2, 3, 17);
        let payload = b"hello-chunk-bytes-1234567890".to_vec();
        store.save(coord, &payload).expect("save");
        store.flush().expect("flush");
        let loaded = store.load(coord).expect("load");
        assert_eq!(loaded.as_deref(), Some(payload.as_slice()));
        // Missing coords return None.
        let missing = store
            .load(Vec3i::new(99, 99, 99))
            .expect("load missing");
        assert!(missing.is_none());
        drop(store);
    }
}

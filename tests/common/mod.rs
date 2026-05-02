//! Shared test fixtures for integration tests under `rs/tests/`.
//!
//! Cargo treats `tests/common/mod.rs` specially — it does not get its own
//! test target. Each integration test binary that wants these helpers
//! includes it via `mod common;`.
//!
//! [`ScratchDir`] hands out a unique absolute path under
//! `std::env::temp_dir()` and best-effort cleans it up on drop. Tests
//! pass that path to `World::new_at` / `TilesStore::open_at` so they
//! don't have to chdir — which means `cargo test` can run multiple
//! integration test binaries in parallel without the cwd race we hit
//! before.

use std::path::PathBuf;
use std::sync::atomic::{AtomicU64, Ordering};

/// Test scratch directory in the OS temp dir. The contained path is
/// absolute; pass it to `World::new_at` etc. — no `chdir` happens.
/// `Drop` does best-effort cleanup of the entire subtree.
pub struct ScratchDir {
    path: PathBuf,
}

impl ScratchDir {
    /// Returns a fresh empty directory tagged with `tag`, the process id,
    /// and a monotonic counter so concurrent tests get distinct paths.
    pub fn new(tag: &str) -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(0);
        let n = COUNTER.fetch_add(1, Ordering::Relaxed);
        let pid = std::process::id();
        let path = std::env::temp_dir().join(format!("neworld-it-{tag}-{pid}-{n}"));
        let _ = std::fs::remove_dir_all(&path);
        std::fs::create_dir_all(&path).expect("create scratch dir");
        Self { path }
    }

    /// Absolute path to the scratch directory.
    pub fn path(&self) -> &std::path::Path {
        &self.path
    }
}

impl Drop for ScratchDir {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.path);
    }
}

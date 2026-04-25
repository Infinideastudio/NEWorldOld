//! Shared test fixtures for `world::*` modules.
//!
//! `TilesStore::open` is cwd-relative and `sled` rejects concurrent opens of
//! the same DB. Every test that constructs a `World` or a `TilesStore` must
//! therefore (1) chdir into a fresh scratch directory and (2) hold
//! [`TEST_LOCK`] for the duration of the test so that the chdir + `sled::open`
//! pair is observed atomically. A single shared lock avoids the cross-module
//! flake we hit when `world::tests` and `world::store::tests` each held their
//! own static mutex and could race for cwd.

#![cfg(test)]

use std::path::PathBuf;
use std::sync::Mutex;
use std::sync::atomic::{AtomicU64, Ordering};

/// Process-global test mutex. Held by every test that touches `World` or
/// `TilesStore` — the chdir + `sled::open` pair must be serialised across
/// every module under `worlds::world`.
pub(super) static TEST_LOCK: Mutex<()> = Mutex::new(());

/// Test scratch directory in the OS temp dir. `Drop` restores cwd and best-
/// effort removes the directory. Mirrors the `i18n.rs::tests::ScratchDir`
/// pattern so we don't take a `tempfile` dep.
pub(super) struct ScratchDir {
    path: PathBuf,
    prev_cwd: PathBuf,
}

impl ScratchDir {
    pub(super) fn new(tag: &str) -> Self {
        static COUNTER: AtomicU64 = AtomicU64::new(0);
        let n = COUNTER.fetch_add(1, Ordering::Relaxed);
        let pid = std::process::id();
        let path = std::env::temp_dir().join(format!("neworld-world-{tag}-{pid}-{n}"));
        let _ = std::fs::remove_dir_all(&path);
        std::fs::create_dir_all(&path).expect("create scratch dir");
        let prev_cwd = std::env::current_dir().expect("cwd");
        std::env::set_current_dir(&path).expect("chdir into scratch");
        Self { path, prev_cwd }
    }
}

impl Drop for ScratchDir {
    fn drop(&mut self) {
        let _ = std::env::set_current_dir(&self.prev_cwd);
        let _ = std::fs::remove_dir_all(&self.path);
    }
}

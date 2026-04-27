//! `WorldAction` — out-of-band requests from screens up to the [`crate::app::App`].
//!
//! The screen stack returns a [`super::Transition`] each frame; that's the
//! right tool for "push another screen" or "pop me", but it's a poor fit for
//! "load this specific world" or "save and exit to the main menu", which need
//! to coordinate with state the screen layer doesn't own (the live `Game`
//! instance, the worlds-on-disk directory, etc.).
//!
//! `WorldActionQueue` is a single-slot mailbox that the title / world-select /
//! pause screens write into and the app drains at the start of each frame.
//! Single-slot is deliberate — the user can only meaningfully request one
//! lifecycle change per frame; if a second arrives in the same frame it
//! overwrites the first.

use std::path::PathBuf;
use std::sync::{Arc, Mutex};

/// A request from the UI that needs the [`crate::app::App`] to do something
/// with the world lifecycle (which the UI layer can't do on its own).
#[derive(Debug, Clone)]
pub enum WorldAction {
    /// Open or create the world named `name` under `<worlds_root>/<name>/`,
    /// then drop into the in-game UI.
    Enter {
        name: String,
        seed: u32,
    },
    /// Save the current world (if any) and return to the title screen.
    LeaveToTitle,
    /// Permanently delete the on-disk world named `name`.
    Delete {
        name: String,
    },
}

/// Single-slot mailbox shared between the screens (writers) and the app
/// (reader). Wrapped in `Arc<Mutex<_>>` so multiple screens can hold a
/// reference without giving up `Send + Sync`.
#[derive(Default)]
pub struct WorldActionQueue {
    pending: Mutex<Option<WorldAction>>,
}

impl WorldActionQueue {
    /// Construct an empty mailbox.
    #[must_use]
    pub fn new() -> Arc<Self> {
        Arc::new(Self::default())
    }

    /// Submit `action`, overwriting any previously-pending request. Returns
    /// the previous pending action if it was clobbered.
    pub fn submit(&self, action: WorldAction) -> Option<WorldAction> {
        self.pending
            .lock()
            .expect("world action queue poisoned")
            .replace(action)
    }

    /// Take the pending action, if any. Always returns `None` after the first
    /// call until a new `submit` arrives.
    pub fn take(&self) -> Option<WorldAction> {
        self.pending
            .lock()
            .expect("world action queue poisoned")
            .take()
    }
}

/// Helper: the parent directory under which worlds live. `World::new_at`,
/// `World::list_worlds_at`, and `World::delete_world_at` all internally
/// append `"worlds"`, so this returns the parent — the crate dir in dev (so
/// chunk DBs survive between cargo runs without polluting the launch dir),
/// or the cwd in a deployed build (matching the C++ build's behaviour).
/// Net result: worlds live at `<this>/worlds/<name>/`.
#[must_use]
pub fn default_worlds_root() -> PathBuf {
    if let Some(dir) = option_env!("CARGO_MANIFEST_DIR") {
        return PathBuf::from(dir);
    }
    PathBuf::from(".")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn submit_replaces_pending() {
        let q = WorldActionQueue::new();
        assert!(q.submit(WorldAction::LeaveToTitle).is_none());
        let prev = q.submit(WorldAction::Delete {
            name: "x".to_owned(),
        });
        assert!(matches!(prev, Some(WorldAction::LeaveToTitle)));
    }

    #[test]
    fn take_drains_then_returns_none() {
        let q = WorldActionQueue::new();
        q.submit(WorldAction::LeaveToTitle);
        assert!(matches!(q.take(), Some(WorldAction::LeaveToTitle)));
        assert!(q.take().is_none());
    }
}

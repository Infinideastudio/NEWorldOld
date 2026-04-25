//! Process-level setup helpers (currently just `tracing` initialisation).
//!
//! This is *not* the analogue of the C++ `setup.ixx` — that file's window/GL
//! init responsibilities will move to a future `gfx` / `app` module. Here we
//! only handle things that have to happen before any other module runs.

use tracing_subscriber::EnvFilter;
use tracing_subscriber::fmt;
use tracing_subscriber::prelude::*;

/// Initialise the global tracing subscriber.
///
/// Reads the `RUST_LOG` env var (default: `info`). Idempotent — safe to call
/// from tests, where the second call simply errors out and is ignored.
pub fn init_tracing() {
    let filter = EnvFilter::try_from_default_env().unwrap_or_else(|_| EnvFilter::new("info"));
    let layer = fmt::layer().with_target(false).with_level(true);
    let _ = tracing_subscriber::registry()
        .with(filter)
        .with(layer)
        .try_init();
}

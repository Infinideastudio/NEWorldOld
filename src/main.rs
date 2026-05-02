//! `NEWorld` binary entry point.
//!
//! Initialises tracing, logs startup, hands off to [`neworld::app::App`] which
//! owns the winit event loop and the wgpu context.

use tracing_subscriber::EnvFilter;
use tracing_subscriber::fmt;
use tracing_subscriber::prelude::*;

use neworld::app::App;

fn main() {
    let filter = EnvFilter::try_from_default_env().unwrap_or_else(|_| EnvFilter::new("info"));
    let layer = fmt::layer().with_target(false).with_level(true);
    let _ = tracing_subscriber::registry()
        .with(filter)
        .with(layer)
        .try_init();
    tracing::info!("NEWorld {} starting", env!("CARGO_PKG_VERSION"));

    if let Err(err) = App::run() {
        tracing::error!(error = %err, "event loop exited with error");
    }
}

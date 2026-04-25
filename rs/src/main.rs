//! `NEWorld` binary entry point.
//!
//! Initialises tracing, logs startup, hands off to [`neworld::app::App`] which
//! owns the winit event loop and the wgpu context.

use neworld::app::App;
use neworld::setup;

fn main() {
    setup::init_tracing();
    tracing::info!("NEWorld {} starting", env!("CARGO_PKG_VERSION"));

    if let Err(err) = App::run() {
        tracing::error!(error = %err, "event loop exited with error");
    }
}

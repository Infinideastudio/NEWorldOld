use neworld::setup;

fn main() {
    setup::init_tracing();
    tracing::info!("NEWorld {} starting", env!("CARGO_PKG_VERSION"));
}

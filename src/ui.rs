//! Re-export of [`crate::client::ui`] under the legacy `crate::ui` path.
//! The Flutter-style layout engine is render-only — it draws through
//! egui — so it lives in `client/` alongside the rest of the renderer.
//! Existing call sites that say `use crate::ui::Flex` keep working
//! through this shim.

pub use crate::client::ui::*;

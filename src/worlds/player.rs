//! Re-export of [`crate::core::game::player`] under the legacy
//! `crate::worlds::player` path. The player moved into `core/game/`
//! once the simulation half of the engine was carved out — `Player`
//! is server-safe (no GPU dependencies). Existing call sites that say
//! `use crate::worlds::player::Player` keep working through this shim.

pub use crate::core::game::player::*;

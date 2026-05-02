//! Re-export of [`crate::core::blocks`] under the legacy `crate::blocks`
//! path. The real implementation lives in `core/blocks/<submodule>.rs`;
//! this file exists so call sites that grew up against the flat
//! `crate::blocks` module keep compiling while the rest of the codebase
//! migrates.
//!
//! Texture-related items (`BlockTextureIndex`, `BlockTextureRegistry`)
//! are **not** re-exported here — they moved to `crate::client::blocks`
//! when `core` was made server-safe. Render code now imports them from
//! that path.
//!
//! The renamed `Block*` structs are re-exported under their old short
//! names (`Id`, `State`, `Light`, `FaceMapping`, `Orientation`) — `pub
//! use` (not `pub type`) so tuple-constructor syntax like `Id(0)` and
//! `State(7)` keeps working.

pub use crate::core::blocks::*;

pub use crate::core::blocks::BlockFaceMapping as FaceMapping;
pub use crate::core::blocks::BlockId as Id;
pub use crate::core::blocks::BlockLight as Light;
pub use crate::core::blocks::BlockOrientation as Orientation;
pub use crate::core::blocks::BlockState as State;

//! Re-export of [`crate::core::math`] under the legacy `crate::math` path.
//! The module moved into `core/` once the simulation half of the engine
//! was carved out — `Vec3*`, `Aabb*`, `Eulerd`, and `Frustum*` are all
//! server-safe with no GPU dependencies. Existing call sites that say
//! `use crate::math::Vec3i` keep working through this shim.

pub use crate::core::math::*;

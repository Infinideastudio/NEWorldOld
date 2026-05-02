//! Math primitives — `cgmath` re-exports plus generic `Aabb3`, `Euler`, `Frustum`.
//!
//! See `docs/rust_migration.md` §4.1 for the migration spec. We use `cgmath`
//! rather than `glam` because the AABB / Euler / Frustum types here are parametric
//! over the scalar (`f32` / `f64` and beyond), and `cgmath`'s
//! `Vector3<T>` / `Matrix4<T>` keep that genericity. `glam` would force a separate
//! concrete type per scalar (`Vec3` vs `DVec3`) and a macro hack to share code.

mod aabb;
mod euler;
mod frustum;

pub use cgmath::{Matrix2, Matrix3, Matrix4, Vector2, Vector3, Vector4};

pub use aabb::Aabb;
pub use euler::Euler;
pub use frustum::Frustum;

pub type Vec2i = Vector2<i32>;
pub type Vec3i = Vector3<i32>;
pub type Vec4i = Vector4<i32>;
pub type Vec2u = Vector2<u32>;
pub type Vec3u = Vector3<u32>;
pub type Vec4u = Vector4<u32>;
pub type Vec2f = Vector2<f32>;
pub type Vec3f = Vector3<f32>;
pub type Vec4f = Vector4<f32>;
pub type Vec2d = Vector2<f64>;
pub type Vec3d = Vector3<f64>;
pub type Vec4d = Vector4<f64>;
pub type Mat2f = Matrix2<f32>;
pub type Mat2d = Matrix2<f64>;
pub type Mat3f = Matrix3<f32>;
pub type Mat3d = Matrix3<f64>;
pub type Mat4f = Matrix4<f32>;
pub type Mat4d = Matrix4<f64>;
pub type Aabbf = Aabb<f32>;
pub type Aabbd = Aabb<f64>;
pub type Eulerf = Euler<f32>;
pub type Eulerd = Euler<f64>;
pub type Frustumf = Frustum<f32>;
pub type Frustumd = Frustum<f64>;

/// Player and world double-precision positions.
pub type Coord = Vector3<f64>;

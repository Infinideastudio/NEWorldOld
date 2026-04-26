//! Math primitives — `cgmath` re-exports plus generic `Aabb3`, `Euler`, `Frustum`.
//!
//! See `docs/rust_migration.md` §4.1 for the migration spec. We use `cgmath`
//! rather than `glam` because the AABB / Euler / Frustum types here are parametric
//! over the scalar (`f32` / `f64` and beyond), and `cgmath`'s
//! `Vector3<S>` / `Matrix4<S>` keep that genericity. `glam` would force a separate
//! concrete type per scalar (`Vec3` vs `DVec3`) and a macro hack to share code.

pub use cgmath::{
    BaseFloat, BaseNum, Deg, ElementWise, EuclideanSpace, InnerSpace, Matrix, Matrix3, Matrix4,
    One, Point2, Point3, Quaternion, Rad, SquareMatrix, Transform, Vector2, Vector3, Vector4, Zero,
};

// Element-typed aliases used as the primary spelling in this codebase.
pub type Vec2<S> = Vector2<S>;
pub type Vec3<S> = Vector3<S>;
pub type Vec4<S> = Vector4<S>;
pub type Mat3<S> = Matrix3<S>;
pub type Mat4<S> = Matrix4<S>;
pub type Quat<S> = Quaternion<S>;

// C++-style flavour aliases. The C++ code names vectors by element type
// (Vec3i / Vec3f / Vec3d); these aliases match that surface so call sites
// can be ported without renaming.
pub type Vec2i = Vec2<i32>;
pub type Vec3i = Vec3<i32>;
pub type Vec4i = Vec4<i32>;
pub type Vec2u = Vec2<u32>;
pub type Vec3u = Vec3<u32>;
pub type Vec4u = Vec4<u32>;
pub type Vec2f = Vec2<f32>;
pub type Vec3f = Vec3<f32>;
pub type Vec4f = Vec4<f32>;
pub type Vec2d = Vec2<f64>;
pub type Vec3d = Vec3<f64>;
pub type Vec4d = Vec4<f64>;
pub type Mat3f = Mat3<f32>;
pub type Mat3d = Mat3<f64>;
pub type Mat4f = Mat4<f32>;
pub type Mat4d = Mat4<f64>;

/// Player and world double-precision positions.
pub type Coord = Vec3<f64>;

mod aabb;
mod euler;
mod frustum;

pub use aabb::Aabb3;
pub use euler::Euler;
pub use frustum::Frustum;

// Convenience aliases matching the C++ Aabb3f/Aabb3d/Eulerf/Eulerd/Frustumf/Frustumd.
pub type Aabb3f = Aabb3<f32>;
pub type Aabb3d = Aabb3<f64>;
pub type Eulerf = Euler<f32>;
pub type Eulerd = Euler<f64>;
pub type Frustumf = Frustum<f32>;
pub type Frustumd = Frustum<f64>;

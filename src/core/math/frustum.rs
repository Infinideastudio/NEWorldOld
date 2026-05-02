//! View-frustum culling — Gribb/Hartmann plane extraction from an MVP matrix.

use cgmath::{BaseFloat, InnerSpace, Matrix, Matrix4, Vector4};

use super::aabb::Aabb;

#[derive(Clone, Debug)]
pub struct Frustum<T> {
    /// Six clip-plane equations in the order x+, x-, y+, y-, z+, z-.
    pub planes: [Vector4<T>; 6],
}

impl<T: BaseFloat> Frustum<T> {
    /// Extract the six frustum planes from a model-view-projection matrix.
    /// Assumes column-vector convention (`clip = mvp * point`).
    pub fn from_mvp(mvp: &Matrix4<T>) -> Self {
        let r0 = mvp.row(0);
        let r1 = mvp.row(1);
        let r2 = mvp.row(2);
        let r3 = mvp.row(3);
        Self {
            planes: [
                r3 - r0, // X+
                r3 + r0, // X-
                r3 - r1, // Y+
                r3 + r1, // Y-
                r3 - r2, // Z+
                r3 + r2, // Z-
            ],
        }
    }

    /// Conservative AABB visibility test: returns `false` only when the box
    /// lies entirely outside one of the planes.
    pub fn test(&self, box_: &Aabb<T>) -> bool {
        let bmin = box_.min;
        let bmax = box_.max;
        let one = T::one();
        let zero = T::zero();
        let corners = [
            Vector4::new(bmin.x, bmin.y, bmin.z, one),
            Vector4::new(bmax.x, bmin.y, bmin.z, one),
            Vector4::new(bmin.x, bmax.y, bmin.z, one),
            Vector4::new(bmax.x, bmax.y, bmin.z, one),
            Vector4::new(bmin.x, bmin.y, bmax.z, one),
            Vector4::new(bmax.x, bmin.y, bmax.z, one),
            Vector4::new(bmin.x, bmax.y, bmax.z, one),
            Vector4::new(bmax.x, bmax.y, bmax.z, one),
        ];
        for plane in &self.planes {
            if !corners.iter().any(|v| plane.dot(*v) > zero) {
                return false;
            }
        }
        true
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use cgmath::{Matrix4, Vector3, Vector4};

    /// Reverse-Z perspective matching the C++ `Mat4::perspective_rev`.
    /// Built column-major (cgmath convention). Row-form intent:
    ///   [ f/aspect  0  0      0      ]
    ///   [ 0         f  0      0      ]
    ///   [ 0         0  1      2*near ]
    ///   [ 0         0  -1     0      ]
    fn perspective_rev(fov: f32, aspect: f32, near: f32) -> Matrix4<f32> {
        let f = 1.0 / (fov / 2.0).tan();
        Matrix4::from_cols(
            Vector4::new(f / aspect, 0.0, 0.0, 0.0),
            Vector4::new(0.0, f, 0.0, 0.0),
            Vector4::new(0.0, 0.0, 1.0, -1.0),
            Vector4::new(0.0, 0.0, 2.0 * near, 0.0),
        )
    }

    #[test]
    fn frustum_culls_box_behind_camera() {
        // Camera at origin looking down -Z. Anything with a positive Z (behind
        // the camera) should be culled. We use the perspective_rev MVP only,
        // i.e. the world is in view space already.
        let mvp = perspective_rev(std::f32::consts::FRAC_PI_2, 16.0 / 9.0, 0.1);
        let f = Frustum::<f32>::from_mvp(&mvp);

        // In front of the camera (negative Z) — visible.
        let in_front = Aabb::<f32>::new(
            Vector3::new(-1.0, -1.0, -10.0),
            Vector3::new(1.0, 1.0, -8.0),
        );
        assert!(f.test(&in_front));

        // Behind the camera — should be culled.
        let behind = Aabb::<f32>::new(Vector3::new(-1.0, -1.0, 5.0), Vector3::new(1.0, 1.0, 10.0));
        assert!(!f.test(&behind));

        // Off to the side, far past the X frustum bounds at this depth.
        let way_left = Aabb::<f32>::new(
            Vector3::new(-1000.0, -1.0, -2.0),
            Vector3::new(-100.0, 1.0, -1.0),
        );
        assert!(!f.test(&way_left));
    }

    #[test]
    fn frustum_passes_box_around_origin() {
        let mvp = perspective_rev(std::f32::consts::FRAC_PI_2, 1.0, 0.1);
        let f = Frustum::<f32>::from_mvp(&mvp);
        // Box straddling the camera is conservatively considered visible.
        let around = Aabb::<f32>::new(Vector3::new(-2.0, -2.0, -2.0), Vector3::new(2.0, 2.0, 2.0));
        assert!(f.test(&around));
    }
}

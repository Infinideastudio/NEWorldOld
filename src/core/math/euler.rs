//! Heading/pitch/roll Euler angles. Matches the C++ `Euler<T>` rotation order.

use cgmath::{BaseFloat, Matrix, Matrix4, Quaternion, Rad, Rotation3, Vector3};
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, PartialEq, Serialize, Deserialize)]
pub struct Euler<T> {
    pub heading: T,
    pub pitch: T,
    pub roll: T,
}

impl<T: BaseFloat> Default for Euler<T> {
    fn default() -> Self {
        Self::identity()
    }
}

impl<T: BaseFloat> Euler<T> {
    pub fn new(heading: T, pitch: T, roll: T) -> Self {
        Self {
            heading,
            pitch,
            roll,
        }
    }

    pub fn identity() -> Self {
        Self {
            heading: T::zero(),
            pitch: T::zero(),
            roll: T::zero(),
        }
    }

    /// Wrap each angle into `[-pi, pi]`.
    pub fn normalize(self) -> Self {
        let two_pi = T::from(std::f64::consts::TAU).unwrap();
        let wrap = |a: T| a - (a / two_pi).round() * two_pi;
        Self {
            heading: wrap(self.heading),
            pitch: wrap(self.pitch),
            roll: wrap(self.roll),
        }
    }

    /// Forward unit vector. The identity orientation looks north `-Z`.
    pub fn direction(&self) -> Vector3<T> {
        let (sh, ch) = self.heading.sin_cos();
        let (sp, cp) = self.pitch.sin_cos();
        Vector3::new(-sh * cp, sp, -ch * cp)
    }

    /// Body-to-world rotation: roll (Z), then pitch (X), then heading (Y).
    pub fn matrix(&self) -> Matrix4<T> {
        let r_roll = Matrix4::from_axis_angle(Vector3::unit_z(), Rad(self.roll));
        let r_pitch = Matrix4::from_axis_angle(Vector3::unit_x(), Rad(self.pitch));
        let r_head = Matrix4::from_axis_angle(Vector3::unit_y(), Rad(self.heading));
        r_head * r_pitch * r_roll
    }

    /// World-to-camera matrix — inverse of `matrix()`.
    /// For pure rotations the inverse is the transpose.
    pub fn view_matrix(&self) -> Matrix4<T> {
        self.matrix().transpose()
    }

    pub fn to_quat(&self) -> Quaternion<T> {
        let q_roll = Quaternion::from_axis_angle(Vector3::unit_z(), Rad(self.roll));
        let q_pitch = Quaternion::from_axis_angle(Vector3::unit_x(), Rad(self.pitch));
        let q_head = Quaternion::from_axis_angle(Vector3::unit_y(), Rad(self.heading));
        q_head * q_pitch * q_roll
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use cgmath::{InnerSpace, Vector3, Vector4};
    use std::f64::consts::{FRAC_PI_2, FRAC_PI_4, PI, TAU};

    fn approx(a: Vector3<f64>, b: Vector3<f64>, eps: f64) -> bool {
        (a - b).magnitude() < eps
    }

    #[test]
    fn identity_direction_is_negative_z() {
        let e = Euler::<f64>::identity();
        assert!(approx(e.direction(), Vector3::new(0.0, 0.0, -1.0), 1e-12));
    }

    #[test]
    fn heading_rotates_around_y() {
        // Yaw +pi/2 should turn the forward vector to point along -X
        // (right-handed Y-up, yaw left).
        let e = Euler::<f64>::new(FRAC_PI_2, 0.0, 0.0);
        assert!(approx(e.direction(), Vector3::new(-1.0, 0.0, 0.0), 1e-12));
    }

    #[test]
    fn pitch_up_points_up() {
        let e = Euler::<f64>::new(0.0, FRAC_PI_2, 0.0);
        assert!(approx(e.direction(), Vector3::new(0.0, 1.0, 0.0), 1e-12));
    }

    #[test]
    fn view_matrix_round_trips_direction() {
        // The view matrix takes a world vector to view space. Looking along
        // `direction()` from the origin, the *world* forward direction must
        // map to view-space `-Z` (the camera looks down its own -Z axis).
        let cases = [
            Euler::<f64>::new(0.3, -0.2, 0.0),
            Euler::<f64>::new(-1.4, 0.7, 0.0),
            Euler::<f64>::new(FRAC_PI_4, FRAC_PI_4, 0.0),
        ];
        for e in cases {
            let v = e.view_matrix();
            let dir = e.direction().extend(0.0);
            let in_view = v * dir;
            let truncated = Vector3::new(in_view.x, in_view.y, in_view.z);
            assert!(
                (truncated - Vector3::new(0.0, 0.0, -1.0)).magnitude() < 1e-12,
                "orientation {e:?} mapped {dir:?} -> {in_view:?}, expected -Z"
            );
        }
    }

    #[test]
    fn normalize_wraps_to_pi() {
        let e = Euler::<f64>::new(3.0 * PI, -3.0 * PI, TAU + 0.5).normalize();
        assert!(e.heading.abs() < PI + 1e-12);
        assert!(e.pitch.abs() < PI + 1e-12);
        assert!((e.roll - 0.5).abs() < 1e-12);
    }

    #[test]
    fn quat_and_matrix_agree() {
        let e = Euler::<f64>::new(0.4, -0.2, 0.1);
        let q = e.to_quat();
        let m = e.matrix();
        let v = Vector3::new(1.0, 2.0, 3.0);
        let by_quat = q * v;
        // Treat v as a direction (w=0) so the matrix's translation part has no effect.
        let v4 = Vector4::new(v.x, v.y, v.z, 0.0);
        let mv = m * v4;
        let by_mat = Vector3::new(mv.x, mv.y, mv.z);
        assert!((by_quat - by_mat).magnitude() < 1e-10);
    }
}

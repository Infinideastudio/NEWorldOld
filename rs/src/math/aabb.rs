//! Axis-aligned bounding boxes — port of C++ `AABB<T,3>`.

use core::ops::{Add, AddAssign, Sub, SubAssign};

use cgmath::{BaseFloat, Vector3, Zero};

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Aabb3<S> {
    pub min: Vector3<S>,
    pub max: Vector3<S>,
}

impl<S: BaseFloat> Aabb3<S> {
    #[must_use]
    pub fn new(min: Vector3<S>, max: Vector3<S>) -> Self {
        Self { min, max }
    }

    /// True iff `self` and `other` overlap on axis `i`, with an `eps` slack
    /// shrinking `other`. Mirrors the C++ open-interval test.
    #[must_use]
    #[allow(clippy::many_single_char_names)] // a/b/c/d match the C++ open-interval algorithm
    pub fn intersects_on_axis(&self, i: usize, other: &Self, eps: S) -> bool {
        let a = self.min[i];
        let b = self.max[i];
        let c = other.min[i] + eps;
        let d = other.max[i] - eps;
        (a > c && a < d) || (b > c && b < d) || (c > a && c < b) || (d > a && d < b)
    }

    #[must_use]
    pub fn intersects(&self, other: &Self, eps: S) -> bool {
        (0..3).all(|i| self.intersects_on_axis(i, other, eps))
    }

    /// Single-axis displacement clip against one box. See `clip_displacement`.
    #[must_use]
    pub fn clip_displacement_on_axis(&self, i: usize, other: &Self, disp: S, eps: S) -> S {
        let zero = S::zero();
        for j in 0..3 {
            if j != i && !self.intersects_on_axis(j, other, eps) {
                return disp;
            }
        }
        if disp < zero && self.max[i] > other.min[i] {
            return zero.min(-((self.min[i] - other.max[i]).min(-disp)));
        }
        if disp > zero && self.min[i] < other.max[i] {
            return zero.max((other.min[i] - self.max[i]).min(disp));
        }
        disp
    }

    /// Sweeps `disp` axis-by-axis against `others`, applying each axis's
    /// resolved component before testing the next. The accumulation is
    /// load-bearing for player physics — sliding along walls relies on
    /// step `i+1` seeing the box already moved by step `i`'s clipped value.
    #[must_use]
    pub fn clip_displacement(&self, others: &[Self], mut disp: Vector3<S>, eps: S) -> Vector3<S> {
        let mut t = *self;
        for i in 0..3 {
            let mut d = disp[i];
            for other in others {
                d = t.clip_displacement_on_axis(i, other, d, eps);
            }
            disp[i] = d;
            t.min[i] += d;
            t.max[i] += d;
        }
        disp
    }

    /// Sweep AABB along `delta` — the union of self and self+delta.
    #[must_use]
    pub fn extend(&self, delta: Vector3<S>) -> Self {
        let zero = S::zero();
        let neg = Vector3::new(delta.x.min(zero), delta.y.min(zero), delta.z.min(zero));
        let pos = Vector3::new(delta.x.max(zero), delta.y.max(zero), delta.z.max(zero));
        Self {
            min: self.min + neg,
            max: self.max + pos,
        }
    }
}

impl<S: BaseFloat> Default for Aabb3<S> {
    fn default() -> Self {
        Self {
            min: Vector3::zero(),
            max: Vector3::zero(),
        }
    }
}

impl<S: BaseFloat> Add<Vector3<S>> for Aabb3<S> {
    type Output = Self;
    fn add(self, rhs: Vector3<S>) -> Self {
        Self::new(self.min + rhs, self.max + rhs)
    }
}

impl<S: BaseFloat> Sub<Vector3<S>> for Aabb3<S> {
    type Output = Self;
    fn sub(self, rhs: Vector3<S>) -> Self {
        Self::new(self.min - rhs, self.max - rhs)
    }
}

impl<S: BaseFloat> AddAssign<Vector3<S>> for Aabb3<S> {
    fn add_assign(&mut self, rhs: Vector3<S>) {
        *self = *self + rhs;
    }
}

impl<S: BaseFloat> SubAssign<Vector3<S>> for Aabb3<S> {
    fn sub_assign(&mut self, rhs: Vector3<S>) {
        *self = *self - rhs;
    }
}

impl Aabb3<f32> {
    /// Widen `f32` corners to `f64`.
    #[must_use]
    pub fn to_f64(&self) -> Aabb3<f64> {
        Aabb3::new(
            Vector3::new(
                f64::from(self.min.x),
                f64::from(self.min.y),
                f64::from(self.min.z),
            ),
            Vector3::new(
                f64::from(self.max.x),
                f64::from(self.max.y),
                f64::from(self.max.z),
            ),
        )
    }
}

impl Aabb3<f64> {
    /// Narrow `f64` corners to `f32`.
    #[must_use]
    pub fn to_f32(&self) -> Aabb3<f32> {
        Aabb3::new(
            Vector3::new(self.min.x as f32, self.min.y as f32, self.min.z as f32),
            Vector3::new(self.max.x as f32, self.max.y as f32, self.max.z as f32),
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use cgmath::Vector3;

    #[test]
    fn intersects_basic() {
        let a = Aabb3::<f64>::new(Vector3::new(0.0, 0.0, 0.0), Vector3::new(1.0, 1.0, 1.0));
        let b = Aabb3::<f64>::new(Vector3::new(0.5, 0.5, 0.5), Vector3::new(1.5, 1.5, 1.5));
        let c = Aabb3::<f64>::new(Vector3::new(2.0, 2.0, 2.0), Vector3::new(3.0, 3.0, 3.0));
        assert!(a.intersects(&b, 0.0));
        assert!(!a.intersects(&c, 0.0));
        // Touching at a face is not an intersection (open-interval semantics).
        let d = Aabb3::<f64>::new(Vector3::new(1.0, 0.0, 0.0), Vector3::new(2.0, 1.0, 1.0));
        assert!(!a.intersects(&d, 0.0));
    }

    #[test]
    fn clip_displacement_2d_equivalent() {
        // The intersect test is open-interval, so identical bounds count as
        // non-overlapping. Make wall.y strictly inside mover.y so the
        // 2D-collision intent is preserved on the y axis. z is wide so the
        // third axis is irrelevant.
        let mover = Aabb3::<f64>::new(
            Vector3::new(0.0, 0.0, -100.0),
            Vector3::new(1.0, 1.0, 100.0),
        );
        let wall = Aabb3::<f64>::new(Vector3::new(2.0, 0.1, -50.0), Vector3::new(3.0, 0.9, 50.0));
        // Move +x by 5 — should be clipped to the gap (2.0 - 1.0 = 1.0).
        let clipped = mover.clip_displacement(&[wall], Vector3::new(5.0, 0.0, 0.0), 0.0);
        assert!((clipped.x - 1.0).abs() < 1e-12, "got {}", clipped.x);

        // Move -x — wall is to the right, so unaffected.
        let clipped = mover.clip_displacement(&[wall], Vector3::new(-5.0, 0.0, 0.0), 0.0);
        assert!((clipped.x - -5.0).abs() < 1e-12, "got {}", clipped.x);

        // Y-only motion does not push us into the wall on x (no x overlap), so
        // no clipping happens.
        let clipped = mover.clip_displacement(&[wall], Vector3::new(0.0, 0.5, 0.0), 0.0);
        assert!((clipped.y - 0.5).abs() < 1e-12, "got {}", clipped.y);
    }

    #[test]
    fn clip_displacement_3d_corner_collision() {
        // Player-shaped box trying to walk diagonally into a corner of two blocks.
        // Use slightly-shrunk player bounds so axis-perpendicular overlap counts
        // under the open-interval test (mover/block share faces otherwise).
        let player = Aabb3::<f64>::new(
            Vector3::new(0.05, 0.05, 0.05),
            Vector3::new(0.55, 0.85, 0.55),
        );
        let block_x = Aabb3::<f64>::new(Vector3::new(1.0, 0.0, 0.0), Vector3::new(2.0, 1.0, 1.0));
        let block_z = Aabb3::<f64>::new(Vector3::new(0.0, 0.0, 1.0), Vector3::new(1.0, 1.0, 2.0));
        // Push +x and +z by 1.0 each; both should clip to gap (1.0 - 0.55 = 0.45).
        let clipped =
            player.clip_displacement(&[block_x, block_z], Vector3::new(1.0, 0.0, 1.0), 0.0);
        assert!((clipped.x - 0.45).abs() < 1e-12, "x got {}", clipped.x);
        assert!((clipped.z - 0.45).abs() < 1e-12, "z got {}", clipped.z);

        // Falling into the floor.
        let floor = Aabb3::<f64>::new(
            Vector3::new(-10.0, -1.0, -10.0),
            Vector3::new(10.0, 0.0, 10.0),
        );
        let player_above =
            Aabb3::<f64>::new(Vector3::new(0.05, 0.5, 0.05), Vector3::new(0.55, 2.3, 0.55));
        let clipped = player_above.clip_displacement(&[floor], Vector3::new(0.0, -2.0, 0.0), 0.0);
        assert!((clipped.y - -0.5).abs() < 1e-12, "y got {}", clipped.y);
    }

    #[test]
    fn extend_grows_box_in_motion_direction() {
        let b = Aabb3::<f64>::new(Vector3::new(0.0, 0.0, 0.0), Vector3::new(1.0, 1.0, 1.0));
        let swept = b.extend(Vector3::new(2.0, -3.0, 0.0));
        assert_eq!(swept.min, Vector3::new(0.0, -3.0, 0.0));
        assert_eq!(swept.max, Vector3::new(3.0, 1.0, 1.0));
    }

    #[test]
    fn translate_via_add_sub() {
        let b = Aabb3::<f64>::new(Vector3::new(0.0, 0.0, 0.0), Vector3::new(1.0, 1.0, 1.0));
        let shifted = b + Vector3::new(1.0, 2.0, 3.0);
        assert_eq!(shifted.min, Vector3::new(1.0, 2.0, 3.0));
        assert_eq!(shifted.max, Vector3::new(2.0, 3.0, 4.0));
        assert_eq!(shifted - Vector3::new(1.0, 2.0, 3.0), b);
    }

    #[test]
    fn convert_f32_f64() {
        let bf = Aabb3::<f32>::new(Vector3::new(0.0, 1.0, 2.0), Vector3::new(3.0, 4.0, 5.0));
        let bd = bf.to_f64();
        assert_eq!(bd.min, Vector3::new(0.0, 1.0, 2.0));
        assert_eq!(bd.to_f32(), bf);
    }
}

//! View camera — Y-up yaw/pitch + perspective projection, driven by the
//! [`crate::worlds::Player`].
//!
//! Camera is a pure view transform: position and orientation are pushed in by
//! [`super::Game`] each frame after consuming player input + physics. There
//! is no per-camera input state, no movement integration, no FOV easing.
//! Mirrors the C++ `view_matrix` / `perspective` math in `neworld.ixx`.

use cgmath::{InnerSpace, Matrix4, Point3, Rad, Vector3};

use crate::math::{Eulerd, Vec3d};

/// `Matrix4` that maps GL clip space `Z in [-1, 1]` into wgpu's `[0, 1]`
/// **with the depth axis reversed** — near maps to 1, far maps to 0. Combined
/// with `CompareFunction::Greater` and a 0.0 depth clear in the chunk +
/// particle pipelines, this gives reversed-Z's much-better far-plane precision
/// without changing the projection formula. Output column-major form, so
/// reading rows: row 2 emits `-0.5*z + 0.5*w`, i.e. `(1 - gl_z) / 2`.
#[rustfmt::skip]
pub const OPENGL_TO_WGPU_REVERSED: Matrix4<f32> = Matrix4::new(
    1.0, 0.0,  0.0, 0.0,
    0.0, 1.0,  0.0, 0.0,
    0.0, 0.0, -0.5, 0.0,
    0.0, 0.0,  0.5, 1.0,
);

/// `glm`-style Y-up free-look camera, driven externally by [`super::Game`].
///
/// Yaw rotates around the world `+Y`; positive yaw turns left (CCW from
/// above). Pitch is around the camera's local X axis; positive looks up.
/// Both are radians and mirror `Eulerd::heading` / `Eulerd::pitch`.
#[derive(Debug, Clone)]
pub struct Camera {
    /// World-space eye position (`player.look_coord()` plus interpolation
    /// offset, pushed in by `Game::write_frame_uniforms`).
    pub position: Vec3d,
    pub yaw: f64,
    pub pitch: f64,
    pub fov_y: f32,
    pub near: f32,
    pub far: f32,
}

impl Camera {
    /// Construct a camera at `position` with default orientation + projection.
    /// `Game::tick_render` overwrites all of these from the player each frame,
    /// but the initial values determine the first frame's view.
    #[must_use]
    pub fn new(position: Vec3d) -> Self {
        Self {
            position,
            yaw: 0.0,
            pitch: 0.0,
            fov_y: 70.0_f32.to_radians(),
            near: 0.1,
            far: 1024.0,
        }
    }

    /// Sync orientation from the player's `Eulerd`. Mirrors the C++
    /// `view_matrix` derivation: heading → yaw, pitch → pitch.
    pub fn set_orientation(&mut self, orientation: Eulerd) {
        self.yaw = orientation.heading;
        self.pitch = orientation.pitch;
    }

    /// Unit forward vector in world space.
    #[must_use]
    pub fn forward(&self) -> Vec3d {
        let cy = self.yaw.cos();
        let sy = self.yaw.sin();
        let cp = self.pitch.cos();
        let sp = self.pitch.sin();
        Vec3d::new(-sy * cp, sp, -cy * cp)
    }

    /// Unit right vector (perpendicular to forward in the horizontal plane).
    #[must_use]
    pub fn right(&self) -> Vec3d {
        let cy = self.yaw.cos();
        let sy = self.yaw.sin();
        Vec3d::new(cy, 0.0, -sy)
    }

    /// Right-handed view matrix.
    #[must_use]
    pub fn view_matrix(&self) -> Matrix4<f32> {
        let eye = Point3::new(
            self.position.x as f32,
            self.position.y as f32,
            self.position.z as f32,
        );
        let f = self.forward();
        let dir = Vector3::new(f.x as f32, f.y as f32, f.z as f32).normalize();
        Matrix4::look_to_rh(eye, dir, Vector3::unit_y())
    }

    /// Reversed-Z perspective projection in wgpu's clip-space convention
    /// (`Z in [0, 1]`, near = 1, far = 0). Pair with `CompareFunction::Greater`
    /// and a 0.0 depth clear at the render-pass level.
    #[must_use]
    pub fn proj_matrix(&self, aspect: f32) -> Matrix4<f32> {
        OPENGL_TO_WGPU_REVERSED
            * cgmath::perspective(Rad(self.fov_y), aspect, self.near, self.far)
    }
}

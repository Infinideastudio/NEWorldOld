//! Free-fly camera — Y-up yaw/pitch + perspective projection.
//!
//! WSAD + Space/Shift movement, mouse-look at a fixed sensitivity. Owns
//! its own field-of-view, near/far planes, and movement speed; consumed
//! by [`Game::tick`](super::Game::tick) once per simulation step.

use cgmath::{InnerSpace, Matrix4, Point3, Rad, Vector3};

use crate::input::{InputState, Key};
use crate::math::Vec3d;

/// `Matrix4` that maps GL clip space `Z in [-1, 1]` to wgpu's `[0, 1]`.
/// Pre-multiply against any `cgmath::perspective` result.
#[rustfmt::skip]
pub const OPENGL_TO_WGPU: Matrix4<f32> = Matrix4::new(
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 0.5, 0.0,
    0.0, 0.0, 0.5, 1.0,
);

/// `glm`-style Y-up free-fly camera.
///
/// Yaw rotates around the world `+Y`; positive yaw turns left (CCW from
/// above). Pitch is around the camera's local X axis; positive looks up.
/// Both are radians.
#[derive(Debug, Clone)]
pub struct Camera {
    pub position: Vec3d,
    pub yaw: f64,
    pub pitch: f64,
    pub fov_y: f32,
    pub near: f32,
    pub far: f32,
    /// Movement speed in blocks/second.
    pub speed: f64,
    /// Mouse sensitivity in radians per pixel of motion.
    pub mouse_sensitivity: f64,
}

impl Camera {
    #[must_use]
    pub fn new(position: Vec3d) -> Self {
        Self {
            position,
            yaw: 0.0,
            pitch: -0.35,
            fov_y: 70.0_f32.to_radians(),
            near: 0.1,
            far: 1024.0,
            speed: 18.0,
            mouse_sensitivity: 0.0025,
        }
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
        let dir = Vector3::new(f.x as f32, f.y as f32, f.z as f32);
        Matrix4::look_to_rh(eye, dir, Vector3::unit_y())
    }

    /// Perspective projection matrix in wgpu's clip-space convention
    /// (Z in `[0, 1]`).
    #[must_use]
    pub fn proj_matrix(&self, aspect: f32) -> Matrix4<f32> {
        OPENGL_TO_WGPU * cgmath::perspective(Rad(self.fov_y), aspect, self.near, self.far)
    }

    /// Apply WSAD-space-shift movement and pitch/yaw mouse-look.
    pub fn update_from_input(&mut self, input: &InputState, dt: f32) {
        let dx = f64::from(input.mouse_motion.x);
        let dy = f64::from(input.mouse_motion.y);
        self.yaw -= dx * self.mouse_sensitivity;
        self.pitch -= dy * self.mouse_sensitivity;
        let limit = std::f64::consts::FRAC_PI_2 - 0.01;
        self.pitch = self.pitch.clamp(-limit, limit);

        let forward = self.forward();
        let right = self.right();
        let up = Vec3d::new(0.0, 1.0, 0.0);

        let mut dir = Vec3d::new(0.0, 0.0, 0.0);
        if input.is_key_down(Key::W) {
            dir += forward;
        }
        if input.is_key_down(Key::S) {
            dir -= forward;
        }
        if input.is_key_down(Key::D) {
            dir += right;
        }
        if input.is_key_down(Key::A) {
            dir -= right;
        }
        if input.is_key_down(Key::Space) {
            dir += up;
        }
        if input.is_key_down(Key::LeftShift) {
            dir -= up;
        }

        let mag2 = dir.magnitude2();
        if mag2 > 1e-6 {
            dir /= mag2.sqrt();
        }
        let speed = if input.is_key_down(Key::LeftControl) {
            self.speed * 5.0
        } else {
            self.speed
        };
        self.position += dir * speed * f64::from(dt);
    }
}

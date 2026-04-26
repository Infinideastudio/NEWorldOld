//! Uniform buffer scaffolding for the wgpu renderer.
//!
//! Replaces the C++ `block_layout` template machinery (`render/program.ixx`) with
//! plain `#[repr(C)]` `bytemuck::Pod` structs paired with a small typed buffer
//! wrapper. The C++ side encoded interface-block layouts at compile time so the
//! GL bridge could check them; `wgpu` already validates bind-group layouts
//! against shader reflection at runtime, so the only thing the Rust port needs
//! to guarantee is that each struct is blittable and 16-byte aligned (the WGSL
//! uniform-address-space rule).
//!
//! See migration plan §4.10 for context, and §5 task `[C4]`.

use std::marker::PhantomData;

use bytemuck::Pod;

/// Column-major 4x4 float matrix in the layout WGSL expects for a `mat4x4<f32>`.
///
/// We use a raw nested array (not `cgmath::Matrix4<f32>`) because `Matrix4`
/// does not implement `bytemuck::Pod`. cgmath stores matrices column-major, so
/// each inner `[f32; 4]` is one column of the matrix.
pub type Mat4f = [[f32; 4]; 4];

/// Convert a `cgmath::Matrix4<f32>` into the column-major array form expected
/// by WGSL `mat4x4<f32>` uniforms.
///
/// cgmath stores `Matrix4` as four `Vector4<f32>` columns (`x`, `y`, `z`, `w`),
/// matching WGSL's column-major convention; this helper just unpacks them into
/// nested arrays so the result is `bytemuck::Pod`-friendly.
#[must_use]
pub fn mat4_to_array(m: cgmath::Matrix4<f32>) -> Mat4f {
    [m.x.into(), m.y.into(), m.z.into(), m.w.into()]
}

/// Per-frame uniforms shared across passes that need camera / lighting state.
///
/// Layout is chosen so the total size is a multiple of 16 bytes (the WGSL
/// uniform-address-space alignment rule). The trailing `_pad` keeps the struct
/// blittable without `bytemuck::Pod` complaining about implicit tail padding.
///
/// Tier 4 deferred renderer extensions (shadow / SSR / volumetric clouds):
/// the `shadow_*` and `player_coord_*` fields mirror the C++ `Frame` block
/// in `rendering.ixx`. They're populated as zero/identity placeholders today
/// — the shadow map and SSR shaders rely on them being present so the
/// composition shader compiles. The shadow pass that lands next will fill
/// `shadow_view_proj` + `shadow_params`; SSR will read `inv_view_proj` and
/// the player-coord triplet for the C++ "repeat trick" that keeps shaders
/// numerically stable far from the world origin.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, bytemuck::Pod, bytemuck::Zeroable)]
pub struct FrameUniforms {
    pub view: Mat4f,
    pub proj: Mat4f,
    pub view_proj: Mat4f,
    /// Inverse of `view_proj`. The composition shader unprojects screen-
    /// space + depth into world-space (relative to the camera) so per-pixel
    /// lighting / fog have a real position to work with.
    pub inv_view_proj: Mat4f,
    /// Sun's view-projection — populated by the shadow pass (Tier 4 next
    /// step). Currently the identity matrix; composition skips shadow
    /// sampling when `shadow_params.x <= 0`.
    pub shadow_view_proj: Mat4f,
    pub camera_pos: [f32; 4],
    pub sun_dir: [f32; 4],
    pub screen_size: [f32; 2],
    pub time: f32,
    /// Distance (world-space, blocks) at which the chunk shader's distance
    /// fog starts ramping up from `0`.
    pub fog_start: f32,
    /// Distance at which the fog reaches `1.0` (i.e. pure sky color).
    /// Computed each frame from the world's render distance so a larger
    /// view radius doesn't reveal harsh chunk-boundary clipping.
    pub fog_end: f32,
    /// World-space render distance in blocks (`render_distance * 16`). Used
    /// by SSR / volumetric clouds for early-out distance limits.
    pub render_distance: f32,
    /// Padding so the next field (`shadow_params`) lands on a 16-byte
    /// boundary, matching the WGSL `vec4<f32>` alignment requirement.
    _pad_scalars: [f32; 2],
    /// `(shadow_resolution, shadow_distance, shadow_fisheye, _)`. Composition
    /// treats `x <= 0` as "no shadow map yet"; the shadow pipeline writes a
    /// non-zero resolution when it lands.
    pub shadow_params: [f32; 4],
    /// Floor of the camera in integer block coords. Padded to vec4 for WGSL
    /// alignment.
    pub player_coord_int: [i32; 4],
    /// `player_coord_int mod 25600` — repeat trick from C++ that keeps
    /// world-space coordinates in float range even at world-edge distances.
    pub player_coord_mod: [i32; 4],
    /// Fractional part of the camera position; padded to vec4.
    pub player_coord_frac: [f32; 4],
}

/// Per-draw transform uniforms. `normal` is the inverse-transpose of the upper
/// 3x3 of `model`, padded to a `mat4` so we can keep one bind-group layout for
/// every model-space pass.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, bytemuck::Pod, bytemuck::Zeroable)]
pub struct ModelUniforms {
    pub model: Mat4f,
    pub normal: Mat4f,
}

/// Filter-pass uniforms — direct port of the C++ `FilterUniformBlock` in
/// `rendering.ixx`. Consumed by `shaders/filter.wgsl` (Gaussian blur today,
/// expandable to other separable post-process kernels).
///
/// `filter_id` selects the kernel: `1` = horizontal Gaussian, `2` =
/// vertical Gaussian, anything else = black clear. The Gaussian radius
/// / step size / sigma are fed to the inner loop directly so the same
/// shader can drive a tunable blur from the host without rebuilding.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, bytemuck::Pod, bytemuck::Zeroable)]
pub struct FilterUniforms {
    pub buffer_width: f32,
    pub buffer_height: f32,
    pub filter_id: i32,
    pub gaussian_blur_radius: f32,
    pub gaussian_blur_step_size: f32,
    pub gaussian_blur_sigma: f32,
    /// Padding to the WGSL 16-byte uniform-address-space alignment.
    _pad0: f32,
    /// Padding to the WGSL 16-byte uniform-address-space alignment.
    _pad1: f32,
}

/// Typed wrapper around a `wgpu::Buffer` sized exactly for one `T`.
///
/// The wrapper enforces blittability via the `T: Pod` bound and stores no
/// CPU-side copy of the data — callers update the GPU buffer through
/// [`UniformBuffer::write`].
pub struct UniformBuffer<T: Pod> {
    buffer: wgpu::Buffer,
    _marker: PhantomData<T>,
}

impl<T: Pod> UniformBuffer<T> {
    /// Allocate a uniform buffer big enough to hold one `T`.
    ///
    /// The buffer is created with usage `UNIFORM | COPY_DST` and is *not*
    /// mapped at creation. The minimum allocation size is bumped to 16 bytes
    /// to satisfy the WGSL uniform-address-space requirement (a `T` smaller
    /// than 16 bytes is highly unusual but the static asserts at the bottom of
    /// this module catch the common cases).
    #[must_use]
    pub fn new(device: &wgpu::Device, label: &str) -> Self {
        let size = std::mem::size_of::<T>().max(16) as wgpu::BufferAddress;
        let buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some(label),
            size,
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
            mapped_at_creation: false,
        });
        Self {
            buffer,
            _marker: PhantomData,
        }
    }

    /// Upload `value` to the GPU buffer via the queue.
    pub fn write(&self, queue: &wgpu::Queue, value: &T) {
        queue.write_buffer(&self.buffer, 0, bytemuck::bytes_of(value));
    }

    /// Borrow the buffer as a `BindingResource`, suitable for inclusion in a
    /// `BindGroupEntry`.
    #[must_use]
    pub fn binding(&self) -> wgpu::BindingResource<'_> {
        wgpu::BindingResource::Buffer(wgpu::BufferBinding {
            buffer: &self.buffer,
            offset: 0,
            size: None,
        })
    }

    /// Direct access to the underlying `wgpu::Buffer`. Useful when the caller
    /// needs to build a more complex binding (e.g. dynamic offsets).
    #[must_use]
    pub fn buffer(&self) -> &wgpu::Buffer {
        &self.buffer
    }
}

// Compile-time guarantees that every uniform struct meets the WGSL 16-byte
// alignment rule for the uniform address space. If any of these fires, add an
// explicit `_pad` field to the offending struct.
//
// `is_multiple_of` is not const (yet), so we keep the manual `%` form here.
#[allow(clippy::manual_is_multiple_of)]
const _: () = assert!(std::mem::size_of::<FrameUniforms>() % 16 == 0);
#[allow(clippy::manual_is_multiple_of)]
const _: () = assert!(std::mem::size_of::<ModelUniforms>() % 16 == 0);
#[allow(clippy::manual_is_multiple_of)]
const _: () = assert!(std::mem::size_of::<FilterUniforms>() % 16 == 0);

#[cfg(test)]
mod tests {
    use super::{FilterUniforms, FrameUniforms, Mat4f, ModelUniforms, mat4_to_array};
    use cgmath::{Matrix4, SquareMatrix};

    #[test]
    fn frame_uniforms_size_is_multiple_of_16() {
        assert_eq!(std::mem::size_of::<FrameUniforms>() % 16, 0);
    }

    #[test]
    fn model_uniforms_size_is_multiple_of_16() {
        assert_eq!(std::mem::size_of::<ModelUniforms>() % 16, 0);
    }

    #[test]
    fn filter_uniforms_size_is_multiple_of_16() {
        assert_eq!(std::mem::size_of::<FilterUniforms>() % 16, 0);
    }

    #[test]
    fn mat4_to_array_round_trips_identity() {
        let identity = Matrix4::<f32>::identity();
        let expected: Mat4f = [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ];
        assert_eq!(mat4_to_array(identity), expected);
    }

    #[test]
    fn mat4_to_array_preserves_column_major() {
        // Build a non-symmetric matrix from four explicit columns. cgmath's
        // `Matrix4::new` takes column-major arguments (`c0r0, c0r1, c0r2,
        // c0r3, c1r0, ...`), so the resulting matrix has these as its columns
        // verbatim. The array form must therefore be `[col0, col1, col2,
        // col3]`.
        let m = Matrix4::<f32>::new(
            1.0, 2.0, 3.0, 4.0, // column 0
            5.0, 6.0, 7.0, 8.0, // column 1
            9.0, 10.0, 11.0, 12.0, // column 2
            13.0, 14.0, 15.0, 16.0, // column 3
        );
        let expected: Mat4f = [
            [1.0, 2.0, 3.0, 4.0],
            [5.0, 6.0, 7.0, 8.0],
            [9.0, 10.0, 11.0, 12.0],
            [13.0, 14.0, 15.0, 16.0],
        ];
        assert_eq!(mat4_to_array(m), expected);
    }
}

//! World↔canonical rotation table for axis-aligned blocks.

use super::{BlockFaceMapping, BlockState};

/// World↔canonical rotation for a block's stored state. Used by the chunk
/// mesher to derive per-corner UVs that rotate consistently with the block.
///
/// For [`BlockFaceMapping::Static`] blocks the orientation is always
/// identity. The mapping for [`BlockFaceMapping::AxisAligned`] consumes the
/// state's *inline* payload (`state.inline_or_zero() % 6`):
///
/// | inline % 6 | placement axis | derivation                     |
/// |------------|----------------|--------------------------------|
/// | 0          | +Y (default)   | identity                       |
/// | 1          | -Y             | 180° around world X            |
/// | 2          | +X             | -90° around world Z            |
/// | 3          | -X             | +90° around world Z            |
/// | 4          | +Z             | +90° around world X            |
/// | 5          | -Z             | -90° around world X            |
///
/// External states (high bit set) fall through to the identity rotation —
/// the lookup-table payload lives off-grid and isn't a placement axis.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct BlockOrientation {
    m: [[i8; 3]; 3],
}

impl BlockOrientation {
    pub const IDENTITY: BlockOrientation = BlockOrientation {
        m: [[1, 0, 0], [0, 1, 0], [0, 0, 1]],
    };

    /// Rotation for an axis-aligned block at the given state.
    #[must_use]
    pub fn for_axis_aligned(state: BlockState) -> BlockOrientation {
        match state.inline_or_zero() % 6 {
            0 => Self::IDENTITY,
            1 => Self {
                m: [[1, 0, 0], [0, -1, 0], [0, 0, -1]],
            },
            2 => Self {
                m: [[0, -1, 0], [1, 0, 0], [0, 0, 1]],
            },
            3 => Self {
                m: [[0, 1, 0], [-1, 0, 0], [0, 0, 1]],
            },
            4 => Self {
                m: [[1, 0, 0], [0, 0, 1], [0, -1, 0]],
            },
            5 => Self {
                m: [[1, 0, 0], [0, 0, -1], [0, 1, 0]],
            },
            _ => Self::IDENTITY,
        }
    }

    /// Pick the orientation for any block: `Static` ignores state and returns
    /// identity; `AxisAligned` dispatches to [`Self::for_axis_aligned`].
    #[must_use]
    pub fn for_block(face_mapping: &BlockFaceMapping, state: BlockState) -> BlockOrientation {
        match face_mapping {
            BlockFaceMapping::Static => Self::IDENTITY,
            BlockFaceMapping::AxisAligned => Self::for_axis_aligned(state),
        }
    }

    /// Apply the linear part to a `f32` direction vector.
    #[inline]
    #[must_use]
    pub fn apply_dir(&self, d: [f32; 3]) -> [f32; 3] {
        let m = &self.m;
        [
            f32::from(m[0][0]) * d[0] + f32::from(m[0][1]) * d[1] + f32::from(m[0][2]) * d[2],
            f32::from(m[1][0]) * d[0] + f32::from(m[1][1]) * d[1] + f32::from(m[1][2]) * d[2],
            f32::from(m[2][0]) * d[0] + f32::from(m[2][1]) * d[1] + f32::from(m[2][2]) * d[2],
        ]
    }

    /// Apply the linear part to an `i32` direction.
    #[inline]
    #[must_use]
    pub fn apply_dir_i(&self, d: [i32; 3]) -> [i32; 3] {
        let m = &self.m;
        [
            i32::from(m[0][0]) * d[0] + i32::from(m[0][1]) * d[1] + i32::from(m[0][2]) * d[2],
            i32::from(m[1][0]) * d[0] + i32::from(m[1][1]) * d[1] + i32::from(m[1][2]) * d[2],
            i32::from(m[2][0]) * d[0] + i32::from(m[2][1]) * d[1] + i32::from(m[2][2]) * d[2],
        ]
    }

    /// Apply the affine rotation to a point in the unit cube. Rotations are
    /// about the cube centre `(0.5, 0.5, 0.5)`.
    #[inline]
    #[must_use]
    pub fn apply_point(&self, p: [f32; 3]) -> [f32; 3] {
        let centred = [p[0] - 0.5, p[1] - 0.5, p[2] - 0.5];
        let r = self.apply_dir(centred);
        [r[0] + 0.5, r[1] + 0.5, r[2] + 0.5]
    }
}

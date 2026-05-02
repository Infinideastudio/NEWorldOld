//! Per-face direction bookkeeping.
//!
//! Face directions are indexed `0..6` in the mesher's order
//! `[+X, -X, +Y, -Y, +Z, -Z]`. This module owns the helpers that map a face
//! direction to a [`BlockInfo::faces`] slot ([`face_index_static`]) and to a
//! primary axis ([`face_axis`]), plus the [`BlockFaceMapping`] enum that
//! selects how a block resolves its faces from a state byte.
//!
//! [`BlockInfo::faces`]: super::BlockInfo::faces

/// How [`BlockInfo::face_for`] resolves a face direction into a
/// [`BlockTextureIndex`] given a block's [`BlockState`].
///
/// New variants can be added without disturbing existing ones — the mesher
/// matches exhaustively, so any forgotten renderer integration flags at
/// compile time.
///
/// [`BlockInfo::face_for`]: super::BlockInfo::face_for
/// [`BlockTextureIndex`]: super::BlockTextureIndex
/// [`BlockState`]: super::BlockState
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum BlockFaceMapping {
    /// State-independent. Face `+Y` reads `faces[0]` (top), `-Y` reads
    /// `faces[2]` (bottom), every other face reads `faces[1]` (side).
    Static,
    /// Block is "axis aligned": `state.0 / 2` selects the cap axis
    /// (`0 = Y`, `1 = X`, `2 = Z`); state values `0..=5` therefore encode
    /// six discrete orientations, two per axis. Faces parallel to the cap
    /// axis read `faces[0]` (cap); the four perpendicular faces read
    /// `faces[1]` (side). `faces[2]` is ignored.
    AxisAligned,
}

/// Map a face direction (`0..6`, `[+X, -X, +Y, -Y, +Z, -Z]`) to a
/// `BlockInfo::faces` slot under the static layout: top = 0, side = 1,
/// bottom = 2.
pub fn face_index_static(face_dir: usize) -> usize {
    match face_dir {
        2 => 0,
        3 => 2,
        _ => 1,
    }
}

/// Primary axis (`0 = X`, `1 = Y`, `2 = Z`) of a face direction (`0..6`).
pub fn face_axis(face_dir: usize) -> u8 {
    match face_dir {
        0 | 1 => 0,
        2 | 3 => 1,
        _ => 2,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn static_layout_matches_legacy_table() {
        assert_eq!(face_index_static(2), 0); // +Y top
        assert_eq!(face_index_static(3), 2); // -Y bottom
        for d in [0usize, 1, 4, 5] {
            assert_eq!(face_index_static(d), 1); // sides
        }
    }

    #[test]
    fn axis_partitions_six_directions() {
        assert_eq!(face_axis(0), 0);
        assert_eq!(face_axis(1), 0);
        assert_eq!(face_axis(2), 1);
        assert_eq!(face_axis(3), 1);
        assert_eq!(face_axis(4), 2);
        assert_eq!(face_axis(5), 2);
    }
}

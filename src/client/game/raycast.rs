//! Voxel raycasting — Amanatides–Woo 3D-DDA over an `&impl BlockView`.
//!
//! [F2] in `docs/rust_migration.md` §5. Mirrors the C++ block-pick logic in
//! `neworld.ixx::find_selection`, but expressed as the standard A&W traversal
//! rather than the C++ "step in `selectPrecision = 32` divisions of the
//! `selectDistance = 5.0` ray" approximation. The maximum distance is kept at
//! 5.0 blocks.

use cgmath::InnerSpace;

use crate::core::game::player::BlockView;
use crate::core::math::{Vec3d, Vec3i};

/// Maximum raycast distance, in blocks. Mirrors C++ `selectDistance`.
pub const RAYCAST_MAX: f64 = 5.0;

/// A block hit returned by [`raycast`].
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Hit {
    /// The hit voxel coord.
    pub coord: Vec3i,
    /// The face the ray entered through, as a unit normal in `±X / ±Y / ±Z`.
    pub normal: Vec3i,
}

/// Walk integer voxels from `origin` along `direction`, returning the first
/// non-air voxel within `max_dist` for which `is_solid` is true.
///
/// `is_solid(view, coord)` decides whether a cell terminates the walk; this
/// lets callers skip e.g. water blocks.
pub fn raycast<V, F>(
    view: &V,
    origin: Vec3d,
    direction: Vec3d,
    max_dist: f64,
    is_solid: F,
) -> Option<Hit>
where
    V: BlockView,
    F: Fn(&V, Vec3i) -> bool,
{
    let len2 = direction.magnitude2();
    if !len2.is_finite() || len2 < 1e-20 {
        return None;
    }
    let dir = direction / len2.sqrt();

    let mut x = origin.x.floor() as i32;
    let mut y = origin.y.floor() as i32;
    let mut z = origin.z.floor() as i32;

    let step_x: i32 = if dir.x > 0.0 {
        1
    } else if dir.x < 0.0 {
        -1
    } else {
        0
    };
    let step_y: i32 = if dir.y > 0.0 {
        1
    } else if dir.y < 0.0 {
        -1
    } else {
        0
    };
    let step_z: i32 = if dir.z > 0.0 {
        1
    } else if dir.z < 0.0 {
        -1
    } else {
        0
    };

    // Distance along the ray to the next axis-aligned grid plane.
    let next_boundary = |o: f64, step: i32, idx: i32| -> f64 {
        if step > 0 {
            f64::from(idx + 1) - o
        } else {
            o - f64::from(idx)
        }
    };
    let mut t_max_x = if step_x != 0 {
        next_boundary(origin.x, step_x, x) / dir.x.abs()
    } else {
        f64::INFINITY
    };
    let mut t_max_y = if step_y != 0 {
        next_boundary(origin.y, step_y, y) / dir.y.abs()
    } else {
        f64::INFINITY
    };
    let mut t_max_z = if step_z != 0 {
        next_boundary(origin.z, step_z, z) / dir.z.abs()
    } else {
        f64::INFINITY
    };

    let t_delta_x = if step_x != 0 {
        1.0 / dir.x.abs()
    } else {
        f64::INFINITY
    };
    let t_delta_y = if step_y != 0 {
        1.0 / dir.y.abs()
    } else {
        f64::INFINITY
    };
    let t_delta_z = if step_z != 0 {
        1.0 / dir.z.abs()
    } else {
        f64::INFINITY
    };

    // Camera might already be inside a solid block.
    if is_solid(view, Vec3i::new(x, y, z)) {
        return Some(Hit {
            coord: Vec3i::new(x, y, z),
            normal: Vec3i::new(0, 0, 0),
        });
    }

    // The first iteration overwrites these before reading; initialised so the
    // compiler doesn't warn about possibly-uninitialised use.
    let mut last_step_axis: i32;
    let mut last_step_sign: i32;

    loop {
        if t_max_x < t_max_y && t_max_x < t_max_z {
            if t_max_x > max_dist {
                return None;
            }
            x += step_x;
            last_step_axis = 0;
            last_step_sign = step_x;
            t_max_x += t_delta_x;
        } else if t_max_y < t_max_z {
            if t_max_y > max_dist {
                return None;
            }
            y += step_y;
            last_step_axis = 1;
            last_step_sign = step_y;
            t_max_y += t_delta_y;
        } else {
            if t_max_z > max_dist {
                return None;
            }
            z += step_z;
            last_step_axis = 2;
            last_step_sign = step_z;
            t_max_z += t_delta_z;
        }

        let coord = Vec3i::new(x, y, z);
        if is_solid(view, coord) {
            // The face the ray entered through is opposite to the step we
            // just took (a +X step crosses the -X face of the new voxel).
            let normal = match last_step_axis {
                0 => Vec3i::new(-last_step_sign, 0, 0),
                1 => Vec3i::new(0, -last_step_sign, 0),
                2 => Vec3i::new(0, 0, -last_step_sign),
                _ => Vec3i::new(0, 0, 0),
            };
            return Some(Hit { coord, normal });
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::core::blocks::{BlockData, BlockId, BlockLight, BlockState};
    use crate::core::math::Aabbd;

    /// Test view: every cell with `y < 0` is solid (id 1); above is air (id 0).
    struct GroundFloor;

    impl BlockView for GroundFloor {
        fn block(&self, coord: Vec3i) -> Option<BlockData> {
            let id = if coord.y < 0 {
                BlockId::new(1)
            } else {
                BlockId::new(0)
            };
            Some(BlockData {
                id,
                state: BlockState::default(),
                light: BlockLight::NONE,
            })
        }
        fn block_or_air(&self, coord: Vec3i) -> BlockData {
            self.block(coord).unwrap()
        }
        fn hitboxes(&self, _box_: Aabbd) -> Option<Vec<Aabbd>> {
            Some(Vec::new())
        }
        fn in_water(&self, _box_: Aabbd) -> Option<bool> {
            Some(false)
        }
    }

    /// Test view: a single solid voxel at `coord`, air everywhere else.
    struct OneBlock(Vec3i);

    impl BlockView for OneBlock {
        fn block(&self, coord: Vec3i) -> Option<BlockData> {
            let id = if coord == self.0 {
                BlockId::new(1)
            } else {
                BlockId::new(0)
            };
            Some(BlockData {
                id,
                state: BlockState::default(),
                light: BlockLight::NONE,
            })
        }
        fn block_or_air(&self, coord: Vec3i) -> BlockData {
            self.block(coord).unwrap()
        }
        fn hitboxes(&self, _: Aabbd) -> Option<Vec<Aabbd>> {
            Some(Vec::new())
        }
        fn in_water(&self, _: Aabbd) -> Option<bool> {
            Some(false)
        }
    }

    fn solid_predicate<V: BlockView>(view: &V, coord: Vec3i) -> bool {
        view.block_or_air(coord).id != BlockId::new(0)
    }

    #[test]
    fn straight_down_hits_floor_top() {
        let hit = raycast(
            &GroundFloor,
            Vec3d::new(0.5, 5.0, 0.5),
            Vec3d::new(0.0, -1.0, 0.0),
            10.0,
            solid_predicate,
        );
        let hit = hit.expect("should hit floor");
        assert_eq!(hit.coord, Vec3i::new(0, -1, 0));
        assert_eq!(hit.normal, Vec3i::new(0, 1, 0));
    }

    #[test]
    fn ray_misses_when_max_distance_too_short() {
        let hit = raycast(
            &GroundFloor,
            Vec3d::new(0.5, 5.0, 0.5),
            Vec3d::new(0.0, -1.0, 0.0),
            1.0,
            solid_predicate,
        );
        assert!(hit.is_none(), "ray should not reach floor at max_dist=1");
    }

    #[test]
    fn ray_into_empty_world_misses() {
        let view = OneBlock(Vec3i::new(100, 100, 100));
        let hit = raycast(
            &view,
            Vec3d::new(0.0, 0.0, 0.0),
            Vec3d::new(1.0, 0.0, 0.0),
            5.0,
            solid_predicate,
        );
        assert!(hit.is_none());
    }

    #[test]
    fn ray_in_pure_x_direction_hits_block_with_minus_x_normal() {
        let view = OneBlock(Vec3i::new(3, 0, 0));
        let hit = raycast(
            &view,
            Vec3d::new(0.5, 0.5, 0.5),
            Vec3d::new(1.0, 0.0, 0.0),
            5.0,
            solid_predicate,
        );
        let hit = hit.expect("should hit block at +X");
        assert_eq!(hit.coord, Vec3i::new(3, 0, 0));
        assert_eq!(hit.normal, Vec3i::new(-1, 0, 0));
    }

    #[test]
    fn ray_starting_inside_solid_returns_zero_normal_immediately() {
        let view = OneBlock(Vec3i::new(0, 0, 0));
        let hit = raycast(
            &view,
            Vec3d::new(0.5, 0.5, 0.5),
            Vec3d::new(1.0, 0.0, 0.0),
            5.0,
            solid_predicate,
        );
        let hit = hit.expect("starts inside the block");
        assert_eq!(hit.coord, Vec3i::new(0, 0, 0));
        assert_eq!(hit.normal, Vec3i::new(0, 0, 0));
    }

    #[test]
    fn predicate_can_skip_water_blocks() {
        let view = OneBlock(Vec3i::new(3, 0, 0));
        let hit = raycast(
            &view,
            Vec3d::new(0.5, 0.5, 0.5),
            Vec3d::new(1.0, 0.0, 0.0),
            10.0,
            // Predicate: nothing is solid (treats the block as water).
            |_, _| false,
        );
        assert!(hit.is_none());
    }
}

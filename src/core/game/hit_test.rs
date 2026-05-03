//! Block-collision queries used by player physics and other gameplay.
//!
//! Port of C++ `worlds/worlds.ixx::World::hitboxes` and `World::in_water`
//! (lines 294-327). Both scan a small world-coord cube around `box_` and
//! consult per-block `solid` / id flags to derive collision data.
//!
//! Reads route through one `ReadTxn` covering the loaded chunks in the
//! scan range. Cells whose chunk is unloaded read as air — matches the
//! C++ behaviour where `block_or_air` returned the air default for
//! missing chunks.
//!
//! The `BlockView` trait that consumes these (in `core::game::player`)
//! lets callers wrap a `World` plus the registry / `BaseBlocks` it
//! lacks awareness of — see `WorldHitView` in `client::game`.

use super::base_blocks::BaseBlocks;
use crate::core::blocks::{BlockData, BlockRegistry};
use crate::core::math::{Aabbd, Vec3d, Vec3i};
use crate::core::world::{WorkingSet, World, chunk_coord};

/// Every solid-block AABB whose cell sits in the cube around `box_`
/// (`±2` cells of padding, matching the C++ original). The caller
/// (`Aabb::clip_displacement`) filters them down to the ones that
/// actually intersect the swept query box.
///
/// Returns `None` if any chunk in the scan cube isn't loaded — the
/// caller can't safely move through a partially-known region, so
/// player / particle ticks should freeze for this step instead of
/// silently treating missing chunks as air (which would let them
/// phase through ungenerated terrain).
pub fn hitboxes(world: &World, registry: &BlockRegistry, box_: Aabbd) -> Option<Vec<Aabbd>> {
    let (lo, hi) = scan_range(box_, 2);
    let mut out = Vec::new();
    let complete = each_block(world, lo, hi, |coord, data| {
        if registry.get(data.id).solid {
            out.push(unit_cube(coord));
        }
    });
    complete.then_some(out)
}

/// `Some(true)` iff `box_` intersects any water or lava block in the
/// cube around it (`±1` cell of padding, matching the C++ original).
/// `None` if any chunk in the scan cube isn't loaded.
pub fn in_water(world: &World, base: &BaseBlocks, box_: Aabbd) -> Option<bool> {
    let (lo, hi) = scan_range(box_, 1);
    let mut hit = false;
    let complete = each_block(world, lo, hi, |coord, data| {
        if hit {
            return;
        }
        if (data.id == base.water || data.id == base.lava)
            && box_.intersects(&unit_cube(coord), 0.0)
        {
            hit = true;
        }
    });
    complete.then_some(hit)
}

fn scan_range(box_: Aabbd, pad: i32) -> (Vec3i, Vec3i) {
    let lo = Vec3i::new(
        box_.min.x.round() as i32 - pad,
        box_.min.y.round() as i32 - pad,
        box_.min.z.round() as i32 - pad,
    );
    let hi = Vec3i::new(
        box_.max.x.round() as i32 + pad,
        box_.max.y.round() as i32 + pad,
        box_.max.z.round() as i32 + pad,
    );
    (lo, hi)
}

fn unit_cube(coord: Vec3i) -> Aabbd {
    let lo = Vec3d::new(f64::from(coord.x), f64::from(coord.y), f64::from(coord.z));
    let hi = Vec3d::new(
        f64::from(coord.x + 1),
        f64::from(coord.y + 1),
        f64::from(coord.z + 1),
    );
    Aabbd::new(lo, hi)
}

/// Iterate every world-coord cell in the inclusive cube `lo..=hi`,
/// invoking `f(coord, data)`. Opens one `ReadTxn` covering every
/// chunk the cube touches.
///
/// Returns `false` (without invoking `f`) if any chunk in the cube
/// isn't loaded. Callers should treat this as "scan inconclusive" —
/// see [`hitboxes`] / [`in_water`] for the rationale.
fn each_block(world: &World, lo: Vec3i, hi: Vec3i, mut f: impl FnMut(Vec3i, BlockData)) -> bool {
    let cc_lo = chunk_coord(lo);
    let cc_hi = chunk_coord(hi);

    let mut all = Vec::new();
    for cx in cc_lo.x..=cc_hi.x {
        for cy in cc_lo.y..=cc_hi.y {
            for cz in cc_lo.z..=cc_hi.z {
                let cc = Vec3i::new(cx, cy, cz);
                if !world.is_loaded(cc) {
                    return false;
                }
                all.push(cc);
            }
        }
    }
    let Ok(txn) = world.begin_read_txn_sync(WorkingSet::List(all)) else {
        return false;
    };
    for x in lo.x..=hi.x {
        for y in lo.y..=hi.y {
            for z in lo.z..=hi.z {
                let coord = Vec3i::new(x, y, z);
                if let Ok(data) = txn.read(coord) {
                    f(coord, data);
                }
            }
        }
    }
    true
}

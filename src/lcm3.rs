//! LCM3 circuit update rules.
//!
//! Implements the rewrite system described in `docs/block_updates.md`:
//!
//! * **Gates** (`wire`, `fork`, `not`, `and`, `or`) at clock `t` fire
//!   when *all inputs* are at clock `t + 1` *and* every output is either
//!   a gate at clock `t` or a register at clock `t + 1`. The cell takes
//!   on `data = f(inputs)` and clock `t + 1`.
//! * **Registers** (`ff`) at clock `t` fire when their input is at
//!   clock `t` and every output is either a gate at clock `t` or a
//!   register at clock `t + 1`. The cell takes on `data = input.data`
//!   and clock `t + 1`.
//!
//! Each cell's state byte packs `(orientation * 2 + data) * 3 + clock`
//! (see [`crate::blocks::OrientationCodec::LCM3`]). Connection between
//! two adjacent cells is symmetric: cell `A`'s face role at direction
//! `d` and cell `B = A + d`'s face role at direction `−d` must be a
//! `(Out, In)` pair for `A → B` (i.e. `A` outputs to `B`, `B` reads `A`
//! as an input). Sides default to:
//!
//! | block       | side role |
//! |-------------|-----------|
//! | `wire`/`ff`/`not` | `Neutral` (no port) |
//! | `fork`            | `Out`     (5 outputs total — top + 4 sides) |
//! | `and`/`or`        | `In`      (5 inputs total — bottom + 4 sides) |
//!
//! All LCM3 blocks have `Out` on canonical `+Y` (the cap, drawn as the
//! `OUT_PORT` texture) and `In` on canonical `-Y` (drawn as
//! `IN_PORT`). Orientation rotates these world-side.
//!
//! ## Driving the rewrite from the block-update queue
//!
//! LCM3 rules are local: they consult only the firing cell and its six
//! face neighbours. The same machinery that drives lighting updates
//! works here too — every change to an LCM3 cell or one of its
//! neighbours enqueues those neighbours for re-evaluation, and
//! [`World::process_block_updates`] runs the LCM3 rule check on each
//! drained cell. There's no full-world scan; we never look at a cell
//! that hasn't been touched.
//!
//! The per-tick rate cap on registers is enforced by a parallel
//! `HashMap<Vec3i, usize>` on `World` (`lcm3_ff_fires`) that counts how
//! many times each `ff` has fired in the current
//! `process_block_updates` call. The map is cleared at the start of
//! each call. Gates have no cap — they re-evaluate freely as inputs
//! change, which models the "wait for the register" behaviour the
//! design doc calls out.
//!
//! [`try_apply`] is the entry point `World` calls per drained cell:
//! pure `&World` access, returns `Some(new_state)` if the rule applies
//! (or `None` if the cell isn't LCM3, has no inputs, or fails a
//! precondition).

use crate::blocks::{BaseBlocks, Id, OrientationCodec, State, canonical_face_id};
use crate::math::Vec3i;
use crate::worlds::world::World;

// ---------- Face roles ----------

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
enum Role {
    Out,
    In,
    Neutral,
}

/// Per-canonical-face role for an LCM3 block id. Canonical face indices
/// follow the mesher's order `[+X, -X, +Y, -Y, +Z, -Z]`.
fn canonical_roles(id: Id, base: &BaseBlocks) -> [Role; 6] {
    // All LCM3 blocks: top (canonical +Y) = Out, bottom (canonical -Y) = In.
    let side_role = if id == base.lcm3_fork {
        Role::Out
    } else if id == base.lcm3_nand {
        // NAND has variable arity (1..=5): bottom + every connected
        // side counts as an input. The 1-input case `nand(a) = NOT(a)`
        // recovers plain inversion when only the bottom is connected.
        Role::In
    } else {
        // wire / ff — sides are decorative only.
        Role::Neutral
    };
    let mut roles = [side_role; 6];
    // face_index_static maps canonical face id 2 / 3 to the top / bottom
    // slots; we set those explicitly. (Non-LCM3 ids fall through to
    // Neutral sides + Out/In top/bottom — harmless because callers gate
    // on `is_lcm3`.)
    roles[2] = Role::Out;
    roles[3] = Role::In;
    roles
}

/// Per-world-face role: the canonical role for whichever canonical face
/// the world face direction maps back to under the cell's orientation.
fn world_roles(id: Id, state: State, base: &BaseBlocks) -> [Role; 6] {
    let orientation = (OrientationCodec::LCM3.read)(state);
    let canon = canonical_roles(id, base);
    let mut world = [Role::Neutral; 6];
    let mut face_dir = 0;
    while face_dir < 6 {
        world[face_dir] = canon[canonical_face_id(orientation, face_dir)];
        face_dir += 1;
    }
    world
}

/// World-axis directions per face id. Same order as `canonical_face_id`.
const NEIGHBOURS: [Vec3i; 6] = [
    Vec3i::new(1, 0, 0),
    Vec3i::new(-1, 0, 0),
    Vec3i::new(0, 1, 0),
    Vec3i::new(0, -1, 0),
    Vec3i::new(0, 0, 1),
    Vec3i::new(0, 0, -1),
];

/// Opposite face id (`+X ↔ -X`, `+Y ↔ -Y`, `+Z ↔ -Z`). Each pair differs
/// only in the sign bit; XOR with 1 flips it.
#[inline]
const fn opposite_face(face_dir: usize) -> usize {
    face_dir ^ 1
}

// ---------- LCM3 state field accessors ----------

#[inline]
const fn state_clock(s: State) -> u8 {
    s.0 % 3
}

#[inline]
const fn state_data(s: State) -> u8 {
    (s.0 / 3) & 1
}

/// Build an LCM3 state from `(orientation_index, data, clock)` per the
/// codec's `(orientation * 2 + data) * 3 + clock` packing.
#[inline]
const fn pack_state(orientation_index: u8, data: u8, clock: u8) -> State {
    State((orientation_index % 8).wrapping_mul(6) + (data & 1) * 3 + (clock % 3))
}

// ---------- Gate function ----------

/// `f(inputs)` — the boolean function the gate carries. Wires and forks
/// are identity (single input expected); NAND folds with NOT-AND
/// semantics, with the 1-input case collapsing to plain inversion
/// (`nand(a) = NOT(a)`).
fn gate_function(id: Id, inputs: &[u8], base: &BaseBlocks) -> u8 {
    if id == base.lcm3_wire || id == base.lcm3_fork || id == base.lcm3_ff {
        // FF takes the same identity-of-input function — the FF rule
        // dispatch in `try_apply` handles its different precondition,
        // not a different `f`.
        inputs.first().copied().unwrap_or(0)
    } else if id == base.lcm3_nand {
        // NAND of empty inputs would be NOT(1) = 0, but the caller
        // already gates on `inputs.len() > 0` (quiescence rule), so
        // we never see the empty case here.
        1 - inputs.iter().fold(1, |a, &b| a & b)
    } else {
        0
    }
}

// ---------- Rule dispatch ----------

/// Try to apply the LCM3 rewrite rule at `coord`. Returns `Some(new_state)`
/// if every precondition holds, `None` otherwise. State-modifying side
/// effects are deferred to the caller; this keeps `try_apply` borrow-safe
/// against `&World` and lets [`World::process_block_updates`] pick what
/// to do with the result (write the cell, count the register fire,
/// advance the queue).
pub fn try_apply(world: &World, coord: Vec3i, base: &BaseBlocks) -> Option<State> {
    let cell = world.block(coord)?;
    if !base.is_lcm3(cell.id) {
        return None;
    }

    let t = state_clock(cell.state);
    let t_plus_1 = (t + 1) % 3;
    let is_register = cell.id == base.lcm3_ff;
    // Gates require their inputs at t+1; registers require theirs at t.
    let required_input_clock = if is_register { t } else { t_plus_1 };
    let roles = world_roles(cell.id, cell.state, base);

    let mut input_data: Vec<u8> = Vec::with_capacity(5);

    for face_dir in 0..6 {
        let role = roles[face_dir];
        if matches!(role, Role::Neutral) {
            continue;
        }
        let nb_coord = coord + NEIGHBOURS[face_dir];
        let Some(nb) = world.block(nb_coord) else {
            // Unloaded neighbour — treat the port as disconnected. Rules
            // can still apply if other ports are connected (and the
            // applicability checks hold for those).
            continue;
        };
        if !base.is_lcm3(nb.id) {
            continue; // non-LCM3 neighbour — no port-port connection.
        }
        let nb_roles = world_roles(nb.id, nb.state, base);
        let nb_role = nb_roles[opposite_face(face_dir)];

        match (role, nb_role) {
            (Role::In, Role::Out) => {
                if state_clock(nb.state) != required_input_clock {
                    return None;
                }
                input_data.push(state_data(nb.state));
            }
            (Role::Out, Role::In) => {
                // Output: must be a gate at t or a register at t+1.
                let expected = if nb.id == base.lcm3_ff { t_plus_1 } else { t };
                if state_clock(nb.state) != expected {
                    return None;
                }
            }
            // Mismatched port roles (Out-Out, In-In) or anything paired
            // with Neutral: not a connection. Skip.
            _ => {}
        }
    }

    // Quiescence: a cell with no connected inputs has nothing to advance
    // on. This avoids "free wires" continuously ticking on their own and
    // matches the intent of a circuit that needs an actual data source
    // (a self-looping FF) to drive forward progress.
    if input_data.is_empty() {
        return None;
    }

    let new_data = gate_function(cell.id, &input_data, base);
    let orientation_index = (OrientationCodec::LCM3.orientation_index)(cell.state);
    Some(pack_state(orientation_index, new_data, t_plus_1))
}

// ---------- Tests ----------

#[cfg(test)]
mod tests {
    use super::*;
    use crate::blocks::{BlockRegistry, Orientation, face_index_static, register_base_blocks};

    /// `face_index_static` maps canonical face id `2`/`3` to top/bottom
    /// slots — the same place LCM3 blocks register `OUT_PORT` /
    /// `IN_PORT`. Verifies the role table aligns with the texture
    /// table.
    #[test]
    fn canonical_roles_align_with_face_slots() {
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        // Top slot for OUT, bottom slot for IN, same convention as
        // `BlockInfo::faces`.
        assert_eq!(face_index_static(2), 0); // top
        assert_eq!(face_index_static(3), 2); // bottom
        let wire_roles = canonical_roles(base.lcm3_wire, &base);
        assert_eq!(wire_roles[2], Role::Out);
        assert_eq!(wire_roles[3], Role::In);
        for &side in &[wire_roles[0], wire_roles[1], wire_roles[4], wire_roles[5]] {
            assert_eq!(side, Role::Neutral);
        }
        let fork_roles = canonical_roles(base.lcm3_fork, &base);
        for face in 0..6 {
            assert_eq!(
                fork_roles[face],
                if face == 3 { Role::In } else { Role::Out },
                "fork face {face}"
            );
        }
        let nand_roles = canonical_roles(base.lcm3_nand, &base);
        for face in 0..6 {
            assert_eq!(
                nand_roles[face],
                if face == 2 { Role::Out } else { Role::In },
                "nand face {face}"
            );
        }
    }

    #[test]
    fn world_roles_rotate_with_orientation() {
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        // Wire with default orientation (state 0 → orientation 0 →
        // identity): OUT on world +Y, IN on world -Y.
        let id = base.lcm3_wire;
        let roles = world_roles(id, State(0), &base);
        assert_eq!(roles[2], Role::Out);
        assert_eq!(roles[3], Role::In);
        // Wire with orientation 2 (+X cap): OUT on world +X, IN on world -X.
        // State `12` packs orientation 2, data 0, clock 0.
        let roles_x = world_roles(id, State(12), &base);
        assert_eq!(roles_x[0], Role::Out);
        assert_eq!(roles_x[1], Role::In);
        // Y / Z faces are sides — Neutral for wire.
        assert_eq!(roles_x[2], Role::Neutral);
    }

    #[test]
    fn pack_state_round_trips_through_codec() {
        // Sanity check that the local `pack_state` matches the codec's
        // own `write` for sample (orientation, data, clock) triples.
        for orientation in 0..6_u8 {
            for data in 0..2_u8 {
                for clock in 0..3_u8 {
                    let packed = pack_state(orientation, data, clock);
                    assert_eq!(state_clock(packed), clock);
                    assert_eq!(state_data(packed), data);
                    assert_eq!(packed.0 / 6, orientation);
                    // Codec round-trip: write(orientation, packed_with_interior_kept)
                    // should equal `packed` (preserves interior).
                    let from_codec = (OrientationCodec::LCM3.write)(orientation, packed);
                    assert_eq!(from_codec, packed);
                }
            }
        }
    }

    #[test]
    fn opposite_face_is_self_inverse() {
        for face in 0..6 {
            assert_eq!(opposite_face(opposite_face(face)), face);
            // Same axis, opposite sign:
            assert_eq!(face / 2, opposite_face(face) / 2);
            assert_ne!(face & 1, opposite_face(face) & 1);
        }
    }

    #[test]
    fn gate_function_truth_tables() {
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        // Wire / fork / ff: identity.
        for id in [base.lcm3_wire, base.lcm3_fork, base.lcm3_ff] {
            assert_eq!(gate_function(id, &[0], &base), 0);
            assert_eq!(gate_function(id, &[1], &base), 1);
        }
        // NAND, 1 input → NOT(a).
        assert_eq!(gate_function(base.lcm3_nand, &[0], &base), 1);
        assert_eq!(gate_function(base.lcm3_nand, &[1], &base), 0);
        // NAND, multi-input → NOT(AND(...)).
        assert_eq!(gate_function(base.lcm3_nand, &[1, 1, 1], &base), 0);
        assert_eq!(gate_function(base.lcm3_nand, &[1, 0, 1], &base), 1);
        assert_eq!(gate_function(base.lcm3_nand, &[0, 0], &base), 1);
    }

    /// Quick sanity check on `Orientation::for_axis_aligned_index`'s
    /// linkage with `canonical_face_id` for the orientation we hard-code
    /// in `world_roles_rotate_with_orientation`. Catches mistakes where
    /// the canonical-face calculation drifts from the rotation table.
    #[test]
    fn orientation_index_2_maps_world_plus_x_to_canonical_plus_y() {
        let o = Orientation::for_axis_aligned_index(2);
        assert_eq!(canonical_face_id(o, 0), 2);
    }
}

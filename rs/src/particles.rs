//! Particle simulation. C++ counterpart: `src/particles.ixx`.
//!
//! Particles live at world-space coordinates and tick in seconds. Collision
//! against the world is consulted via the [`BlockView`] trait (read-only
//! block lookup) so the simulation does not require a `&mut World`.
//!
//! The C++ version applies a fixed-timestep tick (gravity = -0.03 / step,
//! horizontal drag = 0.6 / step, no vertical drag, lifetime measured in
//! integer ticks). The Rust port keeps those constants but exposes a
//! seconds-based [`ParticleSystem::tick`] that scales them to whatever
//! `dt` the caller passes — the simulation rate is the C++ tick rate
//! (30 Hz, 1/30 s per tick).
//!
//! See `docs/rust_migration.md` §4.13 / §5 [D3] for context.
//!
//! Position and velocity are `f64` (world space) so particles share the same
//! coordinate domain as the player and `BlockView::hitboxes`.

use cgmath::Vector3;

use crate::math::{Aabb3d, Vec3d};
use crate::worlds::BlockView;

// ----------------------------------------------------------------------
//   Simulation constants (mirrored from `src/particles.ixx`)
// ----------------------------------------------------------------------

/// Reference simulation step length in seconds. The C++ code applies its
/// drag/gravity constants once per game tick (30 Hz). A `dt` larger or
/// smaller than this scales gravity proportionally and drag exponentially
/// (drag is per-tick multiplicative).
const TICK_SECONDS: f32 = 1.0 / 30.0;

/// Per-tick horizontal velocity multiplier. From `particles.ixx`:
/// `velocity.x() *= 0.6; velocity.z() *= 0.6;`.
const HORIZONTAL_DRAG_PER_TICK: f64 = 0.6;

/// Per-tick gravity acceleration (world units per tick). From
/// `particles.ixx`: `velocity.y() -= 0.03;`.
const GRAVITY_PER_TICK: f64 = 0.03;

/// AABB half-extent of a particle, in world units. The C++ code stores a
/// per-particle `psize` set by `throw_particle`; the Rust port hard-codes a
/// reasonable default (matching the most common spawn site — broken-block
/// debris) so the simulation has no per-particle parameter that doesn't
/// affect physics. Future spawners can override via [`Particle::extent`].
const DEFAULT_EXTENT: f64 = 0.25;

/// Collision epsilon used when calling `Aabb3d::clip_displacement`. Matches
/// `Player::update`.
const COLLISION_EPS: f64 = 1e-8;

// ----------------------------------------------------------------------
//   Particle
// ----------------------------------------------------------------------

#[derive(Clone, Copy, Debug)]
pub struct Particle {
    /// World-space center.
    pub coord: Vector3<f64>,
    /// World-space velocity (units per *tick*, matching the C++ semantics —
    /// `tick` integrates `coord += velocity` once per call after damping).
    pub velocity: Vector3<f64>,
    /// Atlas layer to sample. Re-uses the block diffuse atlas; broken-block
    /// particles inherit the source block's texture layer.
    pub texture_layer: u32,
    /// Sub-rect origin within the source face (`tcx` in the C++ code).
    pub tex_uv: [f32; 2],
    /// AABB half-extent (world units). Defaults to [`DEFAULT_EXTENT`].
    pub extent: f64,
    /// Time alive, in seconds.
    pub age: f32,
    /// Lifetime in seconds; the particle is removed once `age >= max_age`.
    pub max_age: f32,
}

impl Particle {
    /// Convenience constructor with the default extent and zero texture
    /// offset.
    #[must_use]
    pub fn new(coord: Vec3d, velocity: Vec3d, texture_layer: u32, max_age: f32) -> Self {
        Self {
            coord,
            velocity,
            texture_layer,
            tex_uv: [0.0, 0.0],
            extent: DEFAULT_EXTENT,
            age: 0.0,
            max_age,
        }
    }

    /// World-space AABB of this particle.
    #[must_use]
    pub fn aabb(&self) -> Aabb3d {
        let half = Vec3d::new(self.extent, self.extent, self.extent);
        Aabb3d::new(self.coord - half, self.coord + half)
    }
}

// ----------------------------------------------------------------------
//   ParticleSystem
// ----------------------------------------------------------------------

/// Owning collection of live particles, plus the simulation tick.
#[derive(Default)]
pub struct ParticleSystem {
    particles: Vec<Particle>,
}

impl ParticleSystem {
    #[must_use]
    pub fn new() -> Self {
        Self::default()
    }

    pub fn spawn(&mut self, p: Particle) {
        self.particles.push(p);
    }

    #[must_use]
    pub fn particles(&self) -> &[Particle] {
        &self.particles
    }

    #[must_use]
    pub fn len(&self) -> usize {
        self.particles.len()
    }

    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.particles.is_empty()
    }

    pub fn clear(&mut self) {
        self.particles.clear();
    }

    /// Advance the simulation by `dt` seconds.
    ///
    /// Gravity, drag, and AABB-vs-block collision mirror the C++
    /// `particles.ixx::update`: the horizontal-drag multiplier and the
    /// gravity delta are applied once per [`TICK_SECONDS`] step, and then
    /// the resulting velocity is clipped against world hitboxes. With
    /// `dt = TICK_SECONDS` the constants exactly match the C++ values
    /// (`0.6` drag factor, `-0.03` gravity, lifetime decremented by 1 per
    /// tick); intermediate `dt`s scale them via `powf` / multiplication.
    pub fn tick(&mut self, dt: f32, view: &impl BlockView) {
        if dt <= 0.0 {
            return;
        }
        let ticks = f64::from(dt / TICK_SECONDS);
        let drag = HORIZONTAL_DRAG_PER_TICK.powf(ticks);
        let gravity = GRAVITY_PER_TICK * ticks;

        let mut i = 0;
        while i < self.particles.len() {
            let mut p = self.particles[i];

            // Drag + gravity.
            p.velocity.x *= drag;
            p.velocity.z *= drag;
            p.velocity.y -= gravity;

            // AABB-vs-block collision. Mirrors C++:
            //   particle_box.clip_displacement(world.hitboxes(particle_box.extend(velocity)), velocity)
            let aabb = p.aabb();
            let boxes = view.hitboxes(aabb.extend(p.velocity));
            let clipped = aabb.clip_displacement(&boxes, p.velocity, COLLISION_EPS);

            // Apply (scaled) displacement. The clipped vector is in C++ "per
            // tick" units; with `dt == TICK_SECONDS` (the common case) this
            // is `position += velocity`, matching the C++ original.
            p.coord += clipped * ticks;
            // Drop velocity along axes that hit a wall — same as the C++
            // implicit assignment after `clip_displacement`.
            p.velocity = clipped;

            p.age += dt;
            if p.age >= p.max_age {
                self.particles.swap_remove(i);
            } else {
                self.particles[i] = p;
                i += 1;
            }
        }
    }
}

impl ParticleSystem {
    /// Build an empty `ParticleSystem` with at least `capacity` slots
    /// reserved. Useful for spawn-heavy effects (block break = ~10
    /// particles).
    #[must_use]
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            particles: Vec::with_capacity(capacity),
        }
    }
}

// ======================================================================
//   Tests
// ======================================================================

#[cfg(test)]
mod tests {
    use super::*;
    use cgmath::Zero;

    use crate::blocks::{BlockData, Id, Light, State};
    use crate::math::{Aabb3d, Vec3d, Vec3i};

    /// A minimal in-memory `BlockView` for unit tests. Reports `solid_floor`
    /// as the only solid block — every cell with `y < 0` is solid, every
    /// other cell is air. Mirrors the test pattern used in
    /// `worlds::player::tests`.
    struct GroundFloor;

    impl BlockView for GroundFloor {
        fn block(&self, _coord: Vec3i) -> Option<BlockData> {
            Some(BlockData {
                id: Id::new(0),
                state: State::default(),
                light: Light::NONE,
            })
        }

        fn block_or_air(&self, _coord: Vec3i) -> BlockData {
            BlockData {
                id: Id::new(0),
                state: State::default(),
                light: Light::NONE,
            }
        }

        fn hitboxes(&self, box_: Aabb3d) -> Vec<Aabb3d> {
            // Generate one 1×1×1 hitbox per integer cell with `y < 0` that
            // overlaps `box_`. This mimics what `World::hitboxes` does for a
            // ground-only world.
            let lo_x = box_.min.x.floor() as i32 - 1;
            let hi_x = box_.max.x.ceil() as i32 + 1;
            let lo_y = box_.min.y.floor() as i32 - 1;
            let hi_y = box_.max.y.ceil() as i32 + 1;
            let lo_z = box_.min.z.floor() as i32 - 1;
            let hi_z = box_.max.z.ceil() as i32 + 1;
            let mut out = Vec::new();
            for y in lo_y..=hi_y {
                if y >= 0 {
                    continue;
                }
                for x in lo_x..=hi_x {
                    for z in lo_z..=hi_z {
                        out.push(Aabb3d::new(
                            Vec3d::new(f64::from(x), f64::from(y), f64::from(z)),
                            Vec3d::new(
                                f64::from(x + 1),
                                f64::from(y + 1),
                                f64::from(z + 1),
                            ),
                        ));
                    }
                }
            }
            out
        }

        fn in_water(&self, _box_: Aabb3d) -> bool {
            false
        }
    }

    /// A `BlockView` impl that reports zero solid blocks — every query
    /// returns an empty hitbox list. Useful when we don't want collision
    /// to interfere with the kinematic test.
    struct EmptyWorld;

    impl BlockView for EmptyWorld {
        fn block(&self, _coord: Vec3i) -> Option<BlockData> {
            Some(BlockData {
                id: Id::new(0),
                state: State::default(),
                light: Light::NONE,
            })
        }

        fn block_or_air(&self, _coord: Vec3i) -> BlockData {
            BlockData {
                id: Id::new(0),
                state: State::default(),
                light: Light::NONE,
            }
        }

        fn hitboxes(&self, _box_: Aabb3d) -> Vec<Aabb3d> {
            Vec::new()
        }

        fn in_water(&self, _box_: Aabb3d) -> bool {
            false
        }
    }

    #[test]
    fn new_system_is_empty() {
        let s = ParticleSystem::new();
        assert!(s.is_empty());
        assert_eq!(s.len(), 0);
        assert!(s.particles().is_empty());
    }

    #[test]
    fn spawn_grows_then_clear_resets() {
        let mut s = ParticleSystem::new();
        s.spawn(Particle::new(
            Vec3d::new(0.0, 5.0, 0.0),
            Vec3d::zero(),
            0,
            10.0,
        ));
        s.spawn(Particle::new(
            Vec3d::new(1.0, 5.0, 0.0),
            Vec3d::zero(),
            0,
            10.0,
        ));
        assert_eq!(s.len(), 2);
        s.clear();
        assert!(s.is_empty());
    }

    #[test]
    fn gravity_pulls_down() {
        let mut s = ParticleSystem::new();
        s.spawn(Particle::new(
            Vec3d::new(0.0, 10.0, 0.0),
            Vec3d::zero(),
            0,
            10.0,
        ));
        s.tick(0.1, &EmptyWorld);
        let p = s.particles()[0];
        assert!(
            p.coord.y < 10.0,
            "expected y to drop under gravity, got {}",
            p.coord.y
        );
        // x / z unaffected.
        assert!(p.coord.x.abs() < 1e-12);
        assert!(p.coord.z.abs() < 1e-12);
    }

    #[test]
    fn velocity_advances_position() {
        let mut s = ParticleSystem::new();
        s.spawn(Particle::new(
            Vec3d::new(0.0, 5.0, 0.0),
            Vec3d::new(1.0, 0.0, 0.0),
            0,
            10.0,
        ));
        s.tick(1.0, &EmptyWorld);
        let p = s.particles()[0];
        // With dt=1s the integrator runs 30 ticks. Drag is per-tick
        // multiplicative (0.6), so x advances slightly and the velocity
        // decays geometrically — but the position must move forward by
        // *some* amount. The integral of v0 * 0.6^k over k=0..30 is
        // approximately v0 * (1 - 0.6^30) / (1 - 0.6) ≈ v0 / 0.4 ≈ 2.5
        // *per tick*, scaled by `ticks` it grows further. In practice we
        // simply assert the x coord has advanced and not exploded.
        assert!(
            p.coord.x > 0.0 && p.coord.x < 10.0,
            "expected x to advance but stay bounded, got {}",
            p.coord.x
        );
    }

    #[test]
    fn expires_after_max_age() {
        let mut s = ParticleSystem::new();
        s.spawn(Particle::new(
            Vec3d::new(0.0, 5.0, 0.0),
            Vec3d::zero(),
            0,
            0.05,
        ));
        s.tick(0.05, &EmptyWorld);
        // First tick: age == max_age → removed.
        assert_eq!(s.len(), 0);
    }

    #[test]
    fn expires_after_two_ticks() {
        // age 0 → 0.04 → 0.08, removed on second tick (max_age == 0.05).
        let mut s = ParticleSystem::new();
        s.spawn(Particle::new(
            Vec3d::new(0.0, 5.0, 0.0),
            Vec3d::zero(),
            0,
            0.05,
        ));
        s.tick(0.04, &EmptyWorld);
        assert_eq!(s.len(), 1);
        s.tick(0.04, &EmptyWorld);
        assert_eq!(s.len(), 0);
    }

    #[test]
    fn ground_collision() {
        let mut s = ParticleSystem::new();
        // Spawn just above the floor, with a strong downward velocity.
        // Floor: solid for y < 0 → top of floor at y == 0.
        // Particle extent 0.25 → AABB bottom = coord.y - 0.25.
        s.spawn(Particle::new(
            Vec3d::new(0.5, 0.5, 0.5),
            Vec3d::new(0.0, -10.0, 0.0),
            0,
            10.0,
        ));
        s.tick(1.0 / 30.0, &GroundFloor);
        let p = s.particles()[0];
        // Particle's bottom edge must not penetrate y=0. With extent=0.25
        // that means `coord.y >= 0.25` (sub-pixel collision tolerance is
        // built into the open-interval `intersects` test, so we accept
        // 0.25 +/- 1e-6).
        assert!(
            p.coord.y >= 0.25 - 1e-6,
            "particle clipped through floor, y={}",
            p.coord.y
        );
        // Velocity is *clipped*, not zeroed — the C++ original assigns the
        // clipped vector directly so `velocity.y` retains the residual gap
        // distance (here, -0.25). What matters is that next tick's
        // `coord += velocity * ticks` does NOT drop us further into the
        // floor. With ticks=1 here we'd land at y=0.0 (still on the
        // surface) and another collision pass would clip again. So check:
        // the post-tick |velocity.y| is no larger than the original gap.
        assert!(
            p.velocity.y >= -0.25 - 1e-6,
            "velocity.y should be clipped to gap, got {}",
            p.velocity.y
        );
    }

    #[test]
    fn empty_tick_is_a_noop() {
        let mut s = ParticleSystem::new();
        // No particles — should not panic and should not allocate.
        s.tick(0.016, &EmptyWorld);
        assert!(s.is_empty());
    }

    #[test]
    fn zero_dt_is_a_noop() {
        let mut s = ParticleSystem::new();
        s.spawn(Particle::new(
            Vec3d::new(0.0, 5.0, 0.0),
            Vec3d::new(1.0, 1.0, 1.0),
            0,
            10.0,
        ));
        let before = s.particles()[0];
        s.tick(0.0, &EmptyWorld);
        let after = s.particles()[0];
        assert_eq!(after.coord, before.coord);
        assert_eq!(after.velocity, before.velocity);
        // age is f32; an exact match is fine here because `tick(0.0)`
        // returns early before writing to it.
        assert!((after.age - before.age).abs() < f32::EPSILON);
    }
}

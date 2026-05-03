//! Player — position/velocity/orientation, gamemode, inventory.
//!
//! Port of C++ `worlds/player.ixx` + `worlds/player_impl.cpp`. See
//! `docs/rust_migration.md` §4.5. The save/load surface lives in [`save`].

mod save;

pub use self::save::PlayerError;

use serde::{Deserialize, Serialize};

use crate::core::blocks::{BlockData, BlockId, BlockRegistry};
use crate::core::game::base_blocks::BaseBlocks;
use crate::core::math::{Aabbd, Eulerd, Vec3d, Vec3i};
use crate::items::ItemStack;

/// Read-only block lookup used by player physics. Implemented by the
/// `Game` (which routes through `World` + future hitbox cache); tests
/// implement it inline against fixed contents.
///
/// `hitboxes` and `in_water` return `None` when the scan region
/// straddles unloaded chunks — there isn't enough data to safely
/// resolve collision, and the caller (player physics, particles)
/// should freeze for that step rather than fall back to "no obstacle"
/// (which would let entities phase through ungenerated terrain).
pub trait BlockView {
    fn block(&self, coord: Vec3i) -> Option<BlockData>;
    fn hitboxes(&self, box_: Aabbd) -> Option<Vec<Aabbd>>;
    fn in_water(&self, box_: Aabbd) -> Option<bool>;
}

/// Gameplay mode — survival enforces fall damage and disables flying;
/// creative enables flying and allows `cross_wall` toggling.
#[derive(Copy, Clone, Debug, Default, Eq, PartialEq, Serialize, Deserialize)]
pub enum GameMode {
    #[default]
    Survival,
    Creative,
}

/// Player state.
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Player {
    coord: Vec3d,
    velocity: Vec3d,
    orientation: Eulerd,
    flying: bool,
    cross_wall: bool,
    grounded: bool,
    near_wall: bool,
    in_water: bool,
    running: bool,
    jumps: i32,
    game_mode: GameMode,
    inventory: [[ItemStack; 10]; 4],
    held_item_stack_index: usize,
}

impl Player {
    /// Walking speed (blocks/tick).
    pub const WALK_SPEED: f64 = 0.25;
    /// Sprinting speed (blocks/tick).
    pub const RUN_SPEED: f64 = 0.5;
    /// Maximum mid-air jumps before grounding.
    pub const MAX_JUMPS: i32 = 3;
    /// Eye height above the player's foot coordinate.
    pub const LOOK_HEIGHT: f64 = 1.6;

    // ----- getters -----

    pub fn coord(&self) -> Vec3d {
        self.coord
    }

    /// Eye coordinate: `coord + (0, LOOK_HEIGHT, 0)`.
    pub fn look_coord(&self) -> Vec3d {
        Vec3d::new(self.coord.x, self.coord.y + Self::LOOK_HEIGHT, self.coord.z)
    }

    /// Player hitbox: 0.6 wide, 1.8 tall, centred on the foot x/z.
    pub fn aabb(&self) -> Aabbd {
        Aabbd::new(
            self.coord - Vec3d::new(0.3, 0.0, 0.3),
            self.coord + Vec3d::new(0.3, 1.8, 0.3),
        )
    }

    pub fn velocity(&self) -> Vec3d {
        self.velocity
    }

    pub fn orientation(&self) -> Eulerd {
        self.orientation
    }

    pub fn flying(&self) -> bool {
        self.flying
    }

    pub fn cross_wall(&self) -> bool {
        self.cross_wall
    }

    pub fn grounded(&self) -> bool {
        self.grounded
    }

    pub fn near_wall(&self) -> bool {
        self.near_wall
    }

    pub fn in_water(&self) -> bool {
        self.in_water
    }

    pub fn running(&self) -> bool {
        self.running
    }

    /// `RUN_SPEED` if running, else `WALK_SPEED`.
    pub fn speed(&self) -> f64 {
        if self.running {
            Self::RUN_SPEED
        } else {
            Self::WALK_SPEED
        }
    }

    pub fn game_mode(&self) -> GameMode {
        self.game_mode
    }

    pub fn held_item_stack_index(&self) -> usize {
        self.held_item_stack_index
    }

    /// Reference to the currently-held hotbar slot
    /// (`inventory[3][held_item_stack_index]`).
    pub fn held_item_stack(&self) -> &ItemStack {
        &self.inventory[3][self.held_item_stack_index]
    }

    pub fn inventory_item_stack(&self, row: usize, col: usize) -> &ItemStack {
        &self.inventory[row][col]
    }

    pub fn inventory_item_stack_mut(&mut self, row: usize, col: usize) -> &mut ItemStack {
        &mut self.inventory[row][col]
    }

    // ----- setters -----

    /// Set foot coordinate and zero velocity (matches C++ `set_coord`).
    pub fn set_coord(&mut self, value: Vec3d) {
        self.coord = value;
        self.velocity = Vec3d::new(0.0, 0.0, 0.0);
    }

    /// Reset to the world spawn coord `(0, 128, 0)` and zero velocity.
    /// Port of C++ `Player::spawn` — used by `/suicide` and on initial
    /// player creation.
    pub fn spawn(&mut self) {
        self.set_coord(Vec3d::new(0.0, 128.0, 0.0));
    }

    pub fn set_velocity(&mut self, value: Vec3d) {
        self.velocity = value;
    }

    /// Normalises and clamps pitch to `±π/2` via `Eulerd::normalize_player`.
    pub fn set_orientation(&mut self, value: Eulerd) {
        let half_pi = std::f64::consts::FRAC_PI_2;
        self.orientation = value.normalize();
        self.orientation.pitch = self.orientation.pitch.clamp(-half_pi, half_pi);
    }

    pub fn set_running(&mut self, value: bool) {
        self.running = value;
    }

    /// Only honoured in creative mode (mirrors C++ switch on `_game_mode`).
    pub fn set_cross_wall(&mut self, value: bool) {
        match self.game_mode {
            GameMode::Survival => self.cross_wall = false,
            GameMode::Creative => self.cross_wall = value,
        }
    }

    /// Switches mode and applies the C++ side-effects: Creative forces
    /// `flying = true`; Survival forces both `flying` and `cross_wall` off.
    pub fn set_game_mode(&mut self, mode: GameMode) {
        self.game_mode = mode;
        match self.game_mode {
            GameMode::Survival => {
                self.flying = false;
                self.cross_wall = false;
            }
            GameMode::Creative => {
                self.flying = true;
            }
        }
    }

    /// Clamped to `0..=9`.
    pub fn set_held_item_stack_index(&mut self, index: usize) {
        self.held_item_stack_index = index.min(9);
    }

    // ----- inventory -----

    /// Insert `stack` into the inventory. Iterates rows in reverse, fills
    /// existing same-id stacks first (capped at `MAX_COUNT`), then empty
    /// slots. Returns `true` iff every item fit.
    ///
    /// Refuses to insert `air` (`Id(0)`) or an empty stack — those would
    /// "succeed" by occupying a real slot with junk and breaking the
    /// `slot.empty() ⇔ count == 0` UI assumption (an air slot looks like
    /// it has stuff but every painter / merge helper treats it as empty).
    /// `register_base_blocks` always assigns `Id(0)` to air, so the guard
    /// works without threading `BaseBlocks` through.
    pub fn add_item(&mut self, mut stack: ItemStack) -> bool {
        if stack.empty() || stack.id == BlockId::default() {
            return true;
        }
        let max = ItemStack::MAX_COUNT;
        // Pass 1: top up matching stacks.
        for row in self.inventory.iter_mut().rev() {
            for slot in row.iter_mut() {
                if slot.id == stack.id && slot.count < max {
                    if u16::from(stack.count) + u16::from(slot.count) <= u16::from(max) {
                        slot.count += stack.count;
                        return true;
                    }
                    stack.count -= max - slot.count;
                    slot.count = max;
                }
            }
        }
        // Pass 2: place into empty slots.
        for row in self.inventory.iter_mut().rev() {
            for slot in row.iter_mut() {
                if slot.empty() {
                    slot.id = stack.id;
                    if stack.count <= max {
                        slot.count = stack.count;
                        return true;
                    }
                    slot.count = max;
                    stack.count -= max;
                }
            }
        }
        false
    }

    /// Reset every inventory slot to default (empty, id 0).
    pub fn clear_inventory(&mut self) {
        self.inventory = Default::default();
    }

    // ----- spawn / movement events -----

    /// Jump key handler. `just_pressed` distinguishes a fresh press from a
    /// held key (only fresh presses can consume a mid-air jump).
    pub fn on_jump(&mut self, just_pressed: bool) {
        if self.flying || self.cross_wall {
            self.velocity.y += Self::WALK_SPEED;
        } else if self.in_water {
            self.velocity.y = Self::WALK_SPEED;
        } else if (just_pressed && self.jumps < Self::MAX_JUMPS) || self.grounded {
            self.jumps += 1;
            self.grounded = false;
            self.velocity.y = 0.3;
        }
    }

    /// Crouch key handler — only meaningful when flying, cross-walling, or
    /// in water (the C++ `else` branch is otherwise a no-op).
    pub fn on_crouch(&mut self) {
        if self.flying || self.cross_wall {
            self.velocity.y -= Self::WALK_SPEED;
        } else if self.in_water {
            self.velocity.y = -Self::WALK_SPEED;
        }
    }

    // ----- physics tick -----

    /// One physics tick. Reads blocks via `view`. Port of
    /// `player_impl.cpp::Player::update`, with `world.*` calls swapped for
    /// `view.*`.
    #[allow(clippy::float_cmp)]
    // The pre/post-clip equality checks below detect whether `clip_displacement`
    // touched a component. They are exact-equality on purpose (matching the
    // C++ original) — a clipped axis differs by at least the gap-to-block,
    // far above any FP-rounding tolerance.
    pub fn update(&mut self, view: &impl BlockView) {
        let player_aabb = self.aabb();

        // Pre-flight: if any chunk in the swept-AABB region isn't loaded,
        // freeze the player for this tick. Skipping the velocity damp /
        // gravity step too (rather than only the position update) keeps
        // gravity from accumulating into a launch when chunks finally
        // stream in. This case fires during initial load and any time
        // the player's load window outpaces the streamer.
        let probe = player_aabb.extend(self.velocity);
        if !self.cross_wall && view.hitboxes(probe).is_none() {
            return;
        }

        // Velocity damping + gravity.
        if self.flying || self.cross_wall {
            self.velocity *= 0.8;
        } else if self.in_water {
            self.velocity *= 0.6;
            self.velocity.y -= 0.03;
        } else {
            self.velocity.x *= 0.6;
            self.velocity.z *= 0.6;
            self.velocity.y -= 0.03;
        }

        let velocity_original = self.velocity;
        if !self.cross_wall {
            // Re-query: damping changed the velocity, so the swept region
            // is different. Still bail if it now straddles unloaded
            // chunks (rare, but possible at the load-window edge).
            let Some(boxes) = view.hitboxes(player_aabb.extend(self.velocity)) else {
                self.velocity = velocity_original;
                return;
            };
            self.velocity = player_aabb.clip_displacement(&boxes, self.velocity, 1e-8);
        }

        // Position.
        self.coord += self.velocity;

        // State flags.
        let vertical_hit = self.velocity.y != velocity_original.y;
        self.near_wall =
            velocity_original.x != self.velocity.x || velocity_original.z != self.velocity.z;

        self.grounded = vertical_hit && velocity_original.y < 0.0;

        // The hitbox region (pad 2 + sweep) is a strict superset of the
        // in-water region (pad 1 around the static AABB), so this query
        // always succeeds when the pre-flight check above passed.
        if let Some(in_water) = view.in_water(player_aabb) {
            self.in_water = in_water;
        }

        if self.flying || self.cross_wall || self.grounded {
            self.jumps = 0;
        }
    }

    /// Predicate: would placing block `id` at `coord` be valid right now?
    ///
    /// **Renamed from C++ `put_block`**: the C++ method also calls
    /// `world.put_block(...)` to actually mutate the world. To keep the
    /// borrow story clean in Rust, this method is a *predicate only* — it
    /// validates that the target cell is air (or the placed block isn't
    /// solid / the player has cross-wall) and returns `true` if the caller
    /// may proceed. The caller is responsible for the actual world mutation.
    pub fn validate_block_placement(
        &self,
        view: &impl BlockView,
        coord: Vec3i,
        id: BlockId,
        _base: &BaseBlocks,
        registry: &BlockRegistry,
    ) -> bool {
        let player_aabb = self.aabb();
        let block_min = Vec3d::new(f64::from(coord.x), f64::from(coord.y), f64::from(coord.z));
        let block_max = Vec3d::new(
            f64::from(coord.x + 1),
            f64::from(coord.y + 1),
            f64::from(coord.z + 1),
        );
        let block_aabb = Aabbd::new(block_min, block_max);
        let placed_solid = registry.get(id).solid;
        let target_solid = registry.get(view.block(coord).unwrap_or_default().id).solid;
        let player_clear =
            !player_aabb.intersects(&block_aabb, 0.0) || !placed_solid || self.cross_wall;
        player_clear && !target_solid
    }
}

impl Default for Player {
    fn default() -> Self {
        Self {
            coord: Vec3d::new(0.0, 128.0, 0.0),
            velocity: Vec3d::new(0.0, 0.0, 0.0),
            orientation: Eulerd::identity(),
            flying: false,
            cross_wall: false,
            grounded: false,
            near_wall: false,
            in_water: false,
            running: false,
            jumps: 0,
            game_mode: GameMode::Survival,
            inventory: Default::default(),
            held_item_stack_index: 0,
        }
    }
}

// `save_to`, `load_from`, and `PlayerError` live in the [`save`] submodule.

#[cfg(test)]
#[allow(clippy::float_cmp)] // exact constants and round-trips throughout
mod tests {
    use super::*;
    use crate::core::blocks::{BlockData, BlockRegistry};
    use crate::core::game::base_blocks::register_base_blocks;

    /// `BlockView` returning a single block id everywhere — used by the
    /// `validate_block_placement` test that needs a non-air world. Defined
    /// at module scope to satisfy `clippy::items_after_statements`.
    struct ConstantWorld(BlockId);

    impl BlockView for ConstantWorld {
        fn block(&self, _coord: Vec3i) -> Option<BlockData> {
            Some(BlockData {
                id: self.0,
                ..Default::default()
            })
        }
        fn hitboxes(&self, _box_: Aabbd) -> Option<Vec<Aabbd>> {
            Some(Vec::new())
        }
        fn in_water(&self, _box_: Aabbd) -> Option<bool> {
            Some(false)
        }
    }

    /// Minimal `BlockView` returning all-air, no hitboxes — used for tests
    /// that don't exercise collision.
    struct EmptyWorld;
    impl BlockView for EmptyWorld {
        fn block(&self, _coord: Vec3i) -> Option<BlockData> {
            Some(BlockData::default())
        }
        fn hitboxes(&self, _box_: Aabbd) -> Option<Vec<Aabbd>> {
            Some(Vec::new())
        }
        fn in_water(&self, _box_: Aabbd) -> Option<bool> {
            Some(false)
        }
    }

    #[test]
    fn default_player_is_at_spawn() {
        let p = Player::default();
        assert_eq!(p.coord(), Vec3d::new(0.0, 128.0, 0.0));
        assert_eq!(p.game_mode(), GameMode::Survival);
        assert!(!p.flying());
        assert!(!p.cross_wall());
    }

    #[test]
    fn look_coord_offsets_y_by_look_height() {
        let p = Player::default();
        let look = p.look_coord();
        assert_eq!(look.x, 0.0);
        assert!((look.y - (128.0 + Player::LOOK_HEIGHT)).abs() < 1e-12);
        assert_eq!(look.z, 0.0);
    }

    #[test]
    fn set_orientation_clamps_pitch_to_half_pi() {
        let mut p = Player::default();
        p.set_orientation(Eulerd::new(0.0, std::f64::consts::PI, 0.0));
        assert!(p.orientation().pitch.abs() <= std::f64::consts::FRAC_PI_2 + 1e-12);
        p.set_orientation(Eulerd::new(0.0, -std::f64::consts::PI, 0.0));
        assert!(p.orientation().pitch.abs() <= std::f64::consts::FRAC_PI_2 + 1e-12);
    }

    #[test]
    fn set_held_item_stack_index_clamps_to_nine() {
        let mut p = Player::default();
        p.set_held_item_stack_index(7);
        assert_eq!(p.held_item_stack_index(), 7);
        p.set_held_item_stack_index(99);
        assert_eq!(p.held_item_stack_index(), 9);
    }

    #[test]
    fn set_coord_zeros_velocity() {
        let mut p = Player::default();
        p.set_velocity(Vec3d::new(1.0, 2.0, 3.0));
        p.set_coord(Vec3d::new(10.0, 20.0, 30.0));
        assert_eq!(p.velocity(), Vec3d::new(0.0, 0.0, 0.0));
        assert_eq!(p.coord(), Vec3d::new(10.0, 20.0, 30.0));
    }

    #[test]
    fn game_mode_switches_apply_side_effects() {
        let mut p = Player::default();
        p.set_game_mode(GameMode::Creative);
        assert!(p.flying());
        // cross_wall only honoured in creative
        p.set_cross_wall(true);
        assert!(p.cross_wall());
        // Switching back forces both off
        p.set_game_mode(GameMode::Survival);
        assert!(!p.flying());
        assert!(!p.cross_wall());
    }

    #[test]
    fn set_cross_wall_is_noop_in_survival() {
        let mut p = Player::default();
        // Default mode is Survival.
        p.set_cross_wall(true);
        assert!(!p.cross_wall());
    }

    #[test]
    fn add_item_fills_existing_same_id_stacks_then_empty_slots() {
        let mut p = Player::default();
        // Pre-seed bottom-right slot with id=3 count=200.
        *p.inventory_item_stack_mut(3, 9) = ItemStack::new(BlockId::new(3), 200);
        // Add 50 more → fits, all in same slot (200 + 50 = 250 ≤ 255).
        assert!(p.add_item(ItemStack::new(BlockId::new(3), 50)));
        assert_eq!(p.inventory_item_stack(3, 9).count, 250);
        // Add 100 more → fills the seeded slot to 255, spills 95 to the
        // first empty slot. Empty pass walks rows in reverse but slots
        // within a row forwards, so the first empty slot is (3, 0).
        assert!(p.add_item(ItemStack::new(BlockId::new(3), 100)));
        assert_eq!(p.inventory_item_stack(3, 9).count, 255);
        assert_eq!(p.inventory_item_stack(3, 0).id, BlockId::new(3));
        assert_eq!(p.inventory_item_stack(3, 0).count, 95);
    }

    #[test]
    fn add_item_rejects_air_and_empty_stacks() {
        // Air (BlockId 0) and zero-count stacks must not occupy real slots.
        // Both call sites should return `true` (no items lost) but leave
        // the inventory untouched.
        let mut p = Player::default();
        assert!(p.add_item(ItemStack::new(BlockId::new(0), 5)));
        assert!(p.add_item(ItemStack::new(BlockId::new(7), 0)));
        for row in &p.inventory {
            for slot in row {
                assert!(slot.empty(), "no slot should have been filled");
                assert_eq!(slot.id, BlockId::new(0));
            }
        }
    }

    #[test]
    fn add_item_returns_false_when_inventory_overflows() {
        let mut p = Player::default();
        // Fill every slot with id=1 at MAX_COUNT so the matching pass tops
        // out and the empty pass also has nothing to do.
        for row in &mut p.inventory {
            for slot in row.iter_mut() {
                *slot = ItemStack::new(BlockId::new(1), ItemStack::MAX_COUNT);
            }
        }
        // Try to add 10 more of id=1 — won't fit anywhere.
        assert!(!p.add_item(ItemStack::new(BlockId::new(1), 10)));
    }

    #[test]
    fn add_item_drops_off_remainder_across_multiple_calls() {
        let mut p = Player::default();
        // Add 200, then 200 — should land in two empty slots (or one slot
        // and a spill, depending on traversal). Either way, all 400 must be
        // accounted for in the inventory.
        assert!(p.add_item(ItemStack::new(BlockId::new(5), 200)));
        assert!(p.add_item(ItemStack::new(BlockId::new(5), 200)));
        let total: u32 = p
            .inventory
            .iter()
            .flatten()
            .filter(|s| s.id == BlockId::new(5))
            .map(|s| u32::from(s.count))
            .sum();
        assert_eq!(total, 400);
    }

    #[test]
    fn clear_inventory_resets_every_slot() {
        let mut p = Player::default();
        *p.inventory_item_stack_mut(0, 0) = ItemStack::new(BlockId::new(7), 42);
        p.clear_inventory();
        for row in &p.inventory {
            for slot in row {
                assert!(slot.empty());
                assert_eq!(slot.id, BlockId::new(0));
            }
        }
    }

    #[test]
    fn validate_block_placement_allows_placement_into_air_far_from_player() {
        let mut registry = BlockRegistry::new();
        let base = register_base_blocks(&mut registry);
        let p = Player::default();
        let view = EmptyWorld;
        // Place rock 10 blocks below the player — air target, no hitbox overlap.
        assert!(p.validate_block_placement(
            &view,
            Vec3i::new(0, 100, 0),
            base.rock,
            &base,
            &registry
        ));
    }

    #[test]
    fn validate_block_placement_blocks_when_target_occupied() {
        let mut registry = BlockRegistry::new();
        let base = register_base_blocks(&mut registry);
        let p = Player::default();
        let view = ConstantWorld(base.stone);
        assert!(!p.validate_block_placement(
            &view,
            Vec3i::new(0, 100, 0),
            base.rock,
            &base,
            &registry
        ));
    }

    #[test]
    fn update_applies_gravity_in_empty_world() {
        let mut p = Player::default();
        let view = EmptyWorld;
        let v0 = p.velocity();
        p.update(&view);
        // Empty world, not in water, not flying → velocity.y should drop by 0.03.
        assert!(p.velocity().y < v0.y, "expected gravity-induced drop");
    }
}

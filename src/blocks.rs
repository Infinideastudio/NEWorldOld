//! Block primitives — `Id`, `State`, `Light`, `BlockData`, `BlockInfo`,
//! `BlockRegistry`, `BaseBlocks`, and `register_base_blocks`.
//!
//! Folds the texture-atlas mapping (`Textures::indices` in C++) into
//! `BlockInfo::faces`, per `docs/rust_migration.md` §4.2 / §4.12. The legacy
//! C++ module-level globals (`block_info_registry`, `base_blocks`) are
//! intentionally not ported — the registry is owned by the caller.

use std::borrow::Cow;

use bytemuck::{Pod, Zeroable};
use serde::{Deserialize, Serialize};

/// Sentinel state byte meaning "stored externally" (matches the C++
/// `State(std::nullopt)` constructor).
const STATE_EXTERNAL: u8 = 0xFF;

// ---------- Id ----------

/// A block id. Default is `Id(0)` (typically the `air` block once the
/// registry is populated).
#[repr(C)]
#[derive(
    Copy,
    Clone,
    Debug,
    Eq,
    PartialEq,
    Hash,
    Ord,
    PartialOrd,
    Default,
    Pod,
    Zeroable,
    Serialize,
    Deserialize,
)]
pub struct Id(pub u16);

impl Id {
    /// Smallest representable id.
    pub const MIN: Id = Id(0);
    /// Largest representable id.
    pub const MAX: Id = Id(u16::MAX);

    /// Construct an id from a raw `u16`.
    #[must_use]
    pub const fn new(value: u16) -> Self {
        Self(value)
    }

    /// Raw underlying value.
    #[must_use]
    pub const fn get(self) -> u16 {
        self.0
    }
}

// ---------- State ----------

/// Block state byte. Default `State(0)` is "not empty"; `State(0xFF)` means
/// the state is stored externally (mirrors the C++ `State` semantics in
/// `blocks.ixx`).
#[repr(C)]
#[derive(
    Copy,
    Clone,
    Debug,
    Eq,
    PartialEq,
    Hash,
    Ord,
    PartialOrd,
    Default,
    Pod,
    Zeroable,
    Serialize,
    Deserialize,
)]
pub struct State(pub u8);

impl State {
    /// The "stored externally" sentinel value.
    #[must_use]
    pub const fn external() -> Self {
        Self(STATE_EXTERNAL)
    }

    /// True if this is the external-storage sentinel.
    #[must_use]
    pub const fn is_external(self) -> bool {
        self.0 == STATE_EXTERNAL
    }

    /// Build a state from the C++ `optional<uint8_t>` constructor:
    /// * `None`  → external sentinel.
    /// * `Some(v)` → that value, where `v != 0xFF` (the sentinel value is
    ///   reserved and panics).
    #[must_use]
    pub const fn new(value: Option<u8>) -> Self {
        match value {
            None => Self(STATE_EXTERNAL),
            Some(v) => {
                assert!(v != STATE_EXTERNAL, "State::new(Some(0xFF)) is reserved");
                Self(v)
            }
        }
    }

    /// Inverse of [`State::new`]: returns `None` for the external sentinel.
    #[must_use]
    pub const fn get(self) -> Option<u8> {
        if self.0 == STATE_EXTERNAL {
            None
        } else {
            Some(self.0)
        }
    }
}

// ---------- Light ----------

/// Packed lighting byte: sky in the upper nibble, block in the lower nibble.
/// Each channel is in `0..=15`.
#[repr(C)]
#[derive(
    Copy,
    Clone,
    Debug,
    Eq,
    PartialEq,
    Hash,
    Ord,
    PartialOrd,
    Default,
    Pod,
    Zeroable,
    Serialize,
    Deserialize,
)]
pub struct Light(pub u8);

impl Light {
    /// Sky-only full daylight (sky=15, block=0). Mirrors C++ `SKY_LIGHT`.
    pub const SKY: Light = Light::new(15, 0);
    /// Total darkness. Mirrors C++ `NO_LIGHT`.
    pub const NONE: Light = Light::new(0, 0);

    /// Pack `(sky, block)` nibbles. Both must be `<= 15`.
    #[must_use]
    pub const fn new(sky: u8, block: u8) -> Self {
        assert!(sky <= 15, "Light::new: sky > 15");
        assert!(block <= 15, "Light::new: block > 15");
        Self((sky << 4) | (block & 0x0F))
    }

    /// Sky-light component (upper nibble).
    #[must_use]
    pub const fn sky(self) -> u8 {
        self.0 >> 4
    }

    /// Block-light component (lower nibble).
    #[must_use]
    pub const fn block(self) -> u8 {
        self.0 & 0x0F
    }
}

// ---------- BlockData ----------

/// Per-cell payload stored in chunks. `Pod` so chunk arrays can be `memcpy`'d
/// into mesh-input snapshots.
#[repr(C)]
#[derive(Copy, Clone, Debug, Eq, PartialEq, Default, Pod, Zeroable, Serialize, Deserialize)]
pub struct BlockData {
    pub id: Id,
    pub state: State,
    pub light: Light,
}

// ---------- TextureIndex ----------

/// Index into the block texture atlas. Folded into `blocks` rather than living
/// alongside the atlas itself, per the migration plan.
#[repr(C)]
#[derive(
    Copy,
    Clone,
    Debug,
    Eq,
    PartialEq,
    Hash,
    Ord,
    PartialOrd,
    Default,
    Pod,
    Zeroable,
    Serialize,
    Deserialize,
)]
pub struct TextureIndex(pub u16);

impl TextureIndex {
    pub const WHITE: TextureIndex = TextureIndex(0);
    pub const BREAKING_0: TextureIndex = TextureIndex(1);
    pub const BREAKING_1: TextureIndex = TextureIndex(2);
    pub const BREAKING_2: TextureIndex = TextureIndex(3);
    pub const BREAKING_3: TextureIndex = TextureIndex(4);
    pub const BREAKING_4: TextureIndex = TextureIndex(5);
    pub const BREAKING_5: TextureIndex = TextureIndex(6);
    pub const BREAKING_6: TextureIndex = TextureIndex(7);
    pub const BREAKING_7: TextureIndex = TextureIndex(8);
    pub const NULLBLOCK: TextureIndex = TextureIndex(9);
    pub const ROCK: TextureIndex = TextureIndex(10);
    pub const GRASS_TOP: TextureIndex = TextureIndex(11);
    pub const GRASS_SIDE: TextureIndex = TextureIndex(12);
    pub const DIRT: TextureIndex = TextureIndex(13);
    pub const STONE: TextureIndex = TextureIndex(14);
    pub const PLANK: TextureIndex = TextureIndex(15);
    pub const WOOD_TOP: TextureIndex = TextureIndex(16);
    pub const WOOD_SIDE: TextureIndex = TextureIndex(17);
    pub const BEDROCK: TextureIndex = TextureIndex(18);
    pub const LEAF: TextureIndex = TextureIndex(19);
    pub const GLASS: TextureIndex = TextureIndex(20);
    pub const WATER: TextureIndex = TextureIndex(21);
    pub const LAVA: TextureIndex = TextureIndex(22);
    pub const GLOWSTONE: TextureIndex = TextureIndex(23);
    pub const SAND: TextureIndex = TextureIndex(24);
    pub const CEMENT: TextureIndex = TextureIndex(25);
    pub const ICE: TextureIndex = TextureIndex(26);
    pub const COAL: TextureIndex = TextureIndex(27);
    pub const IRON: TextureIndex = TextureIndex(28);
    pub const TNT: TextureIndex = TextureIndex(29);
}

// ---------- FaceMapping ----------

/// How `BlockInfo::face_for` resolves a face direction (one of
/// `[+X, -X, +Y, -Y, +Z, -Z]`) into a [`TextureIndex`] given a block's
/// [`State`].
///
/// Most blocks are state-independent — both `State(0)` and any other byte
/// pick the same texture per face — and use [`FaceMapping::Static`]. Logs
/// (`wood`) and other axis-oriented blocks use [`FaceMapping::AxisAligned`]
/// to read 6 placement orientations out of the state byte.
#[derive(Clone, Debug, PartialEq)]
pub enum FaceMapping {
    /// State-independent. Face `+Y` reads `faces[0]` (top), `-Y` reads
    /// `faces[2]` (bottom), every other face reads `faces[1]` (side) —
    /// the existing `Textures::indices` layout.
    Static,
    /// Block is "axis aligned": `state.0 / 2` selects the cap axis
    /// (`0 = Y`, `1 = X`, `2 = Z`); state values `0..=5` therefore encode
    /// six discrete orientations, two per axis (positive vs negative
    /// pointing — visually identical for a symmetric log, kept distinct
    /// so a future placement system can record direction). Faces parallel
    /// to the cap axis read `faces[0]` (cap); the four perpendicular
    /// faces read `faces[1]` (side). `faces[2]` is ignored.
    AxisAligned,
}

// ---------- BlockInfo ----------

/// Static, registry-owned properties of a block id. Includes the per-face
/// texture mapping (`faces[0..3]` = top, side, bottom) — what was previously
/// `Textures::getTextureIndex` in C++.
#[derive(Clone, Debug, PartialEq)]
pub struct BlockInfo {
    pub name: Cow<'static, str>,
    pub solid: bool,
    pub opaque: bool,
    pub translucent: bool,
    pub hardness: f32,
    /// Texture for `[top, side, bottom]` faces (the order used by the C++
    /// `Textures::indices` table). Replaces `Textures::getTextureIndex`.
    pub faces: [TextureIndex; 3],
    /// How [`Self::face_for`] resolves a face direction + state into a
    /// slot of `faces`. Defaults to [`FaceMapping::Static`] in
    /// `BlockInfo::new_static` / the helper `info(...)` constructor.
    pub face_mapping: FaceMapping,
}

impl BlockInfo {
    /// Texture for a specific face *index* (`0 = top`, `1 = side`,
    /// `2 = bottom`). Returns `NULLBLOCK` if `face >= 3`, matching the
    /// out-of-range fallback in the C++ `getTextureIndex`. Ignores state
    /// — callers that need state-aware lookup (the chunk mesher) use
    /// [`Self::face_for`].
    #[must_use]
    pub fn face(&self, face: usize) -> TextureIndex {
        self.faces
            .get(face)
            .copied()
            .unwrap_or(TextureIndex::NULLBLOCK)
    }

    /// Texture for a face *direction* (0..6, in the mesher's
    /// `[+X, -X, +Y, -Y, +Z, -Z]` order) given the cell's [`State`]. The
    /// state byte is consulted only when [`Self::face_mapping`] is
    /// non-`Static`.
    #[must_use]
    pub fn face_for(&self, face_dir: usize, state: State) -> TextureIndex {
        match self.face_mapping {
            FaceMapping::Static => self.face(face_index_static(face_dir)),
            FaceMapping::AxisAligned => {
                // state 0,1 → Y (default vertical, like the legacy
                // state-0 log); 2,3 → X; 4,5 → Z. `>= 6` falls back to Y
                // so an undefined state still renders a sensible log
                // rather than `NULLBLOCK`.
                let cap_axis: u8 = match (state.0 >> 1) % 3 {
                    1 => 0, // X
                    2 => 2, // Z
                    _ => 1, // Y
                };
                if face_axis(face_dir) == cap_axis {
                    self.face(0) // cap
                } else {
                    self.face(1) // side / bark
                }
            }
        }
    }
}

/// Map a face direction (0..6, `[+X, -X, +Y, -Y, +Z, -Z]`) to a
/// `BlockInfo::faces` slot under the static layout: top = 0, side = 1,
/// bottom = 2. Mirrors the switch in C++ `_merge_face_render_chunk`.
#[inline]
pub fn face_index_static(face_dir: usize) -> usize {
    match face_dir {
        2 => 0,
        3 => 2,
        _ => 1,
    }
}

/// Primary axis (0 = X, 1 = Y, 2 = Z) of a face direction (0..6).
#[inline]
fn face_axis(face_dir: usize) -> u8 {
    match face_dir {
        0 | 1 => 0,
        2 | 3 => 1,
        _ => 2,
    }
}

// ---------- Orientation ----------

/// World↔canonical rotation for a block's stored state. Used by the chunk
/// mesher to derive per-corner UVs that rotate consistently with the
/// block: `state.0 % 6` selects one of six axis-aligned rotations of the
/// unit cube about its centre, so a state-2 (X-axis) log places its bark
/// grain along world X, a state-4 (Z-axis) log along Z, and so on.
///
/// For [`FaceMapping::Static`] blocks, the orientation is always the
/// identity — state is ignored, and the mesher's UV computation collapses
/// to the legacy `FACE_UVS` table. The state-byte→rotation mapping for
/// [`FaceMapping::AxisAligned`] is:
///
/// | state | placement axis | derivation                     |
/// |-------|----------------|--------------------------------|
/// | 0     | +Y (default)   | identity                       |
/// | 1     | -Y             | 180° around world X            |
/// | 2     | +X             | -90° around world Z            |
/// | 3     | -X             | +90° around world Z            |
/// | 4     | +Z             | +90° around world X            |
/// | 5     | -Z             | -90° around world X            |
///
/// Each row encodes a `world→canonical` linear transform — i.e.
/// `canonical[i] = m[i][0]·world[0] + m[i][1]·world[1] + m[i][2]·world[2]`.
/// The affine offset for a unit-cube point is folded into [`Self::apply_point`]
/// via the cube-centre `(0.5, 0.5, 0.5)`.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct Orientation {
    m: [[i8; 3]; 3],
}

impl Orientation {
    /// Identity rotation. `world = canonical`.
    pub const IDENTITY: Orientation = Orientation {
        m: [[1, 0, 0], [0, 1, 0], [0, 0, 1]],
    };

    /// Pick the `world→canonical` rotation for an axis-aligned block at
    /// the given state. `state.0 % 6` keys into the table above; any
    /// other byte (we treat `0..6` as defined and everything else as
    /// undefined) falls back to identity so an unset state still
    /// renders a sensible upright log.
    #[must_use]
    pub fn for_axis_aligned(state: State) -> Orientation {
        match state.0 % 6 {
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

    /// Pick the orientation for any block: `Static` ignores state and
    /// returns identity; `AxisAligned` dispatches to
    /// [`Self::for_axis_aligned`].
    #[must_use]
    pub fn for_block(face_mapping: &FaceMapping, state: State) -> Orientation {
        match face_mapping {
            FaceMapping::Static => Self::IDENTITY,
            FaceMapping::AxisAligned => Self::for_axis_aligned(state),
        }
    }

    /// Apply the linear part of the rotation to a `f32` direction vector
    /// (no translation). Useful for transforming face normals and merge
    /// extension directions.
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

    /// Apply the linear part to an `i32` direction. Convenience wrapper for
    /// face-normal lookups (which start as integer ±1 unit vectors).
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

    /// Apply the affine rotation to a point in the unit cube. Rotations
    /// are about the cube centre `(0.5, 0.5, 0.5)`, so each input
    /// component lands back in `[0, 1]` for inputs in the same range.
    #[inline]
    #[must_use]
    pub fn apply_point(&self, p: [f32; 3]) -> [f32; 3] {
        let centred = [p[0] - 0.5, p[1] - 0.5, p[2] - 0.5];
        let r = self.apply_dir(centred);
        [r[0] + 0.5, r[1] + 0.5, r[2] + 0.5]
    }
}

/// Returned by `BlockRegistry::get` for out-of-range ids — mirrors the C++
/// `_DEFAULT_INFO` const in `BlockInfoRegistry`.
static DEFAULT_INFO: BlockInfo = BlockInfo {
    name: Cow::Borrowed("null"),
    solid: true,
    opaque: true,
    translucent: false,
    hardness: 0.0,
    faces: [
        TextureIndex::NULLBLOCK,
        TextureIndex::NULLBLOCK,
        TextureIndex::NULLBLOCK,
    ],
    face_mapping: FaceMapping::Static,
};

// ---------- BlockRegistry ----------

/// Owning registry of `BlockInfo` entries. Constructed once at boot (typically
/// via `register_base_blocks`) then wrapped in `Arc<BlockRegistry>` for shared
/// read-only access — no module-level globals.
#[derive(Clone, Debug, Default)]
pub struct BlockRegistry {
    entries: Vec<BlockInfo>,
}

impl BlockRegistry {
    /// Empty registry.
    #[must_use]
    pub fn new() -> Self {
        Self {
            entries: Vec::new(),
        }
    }

    /// Append `info` and return its assigned id. Panics if the id space
    /// (`u16`) is exhausted — same failure mode as the C++ version.
    pub fn add(&mut self, info: BlockInfo) -> Id {
        let id = u16::try_from(self.entries.len()).expect("BlockRegistry: id space exhausted");
        self.entries.push(info);
        Id(id)
    }

    /// Look up block info by id. Out-of-range ids return a static `null`
    /// fallback (solid+opaque, hardness 0, all faces `NULLBLOCK`).
    #[must_use]
    pub fn get(&self, id: Id) -> &BlockInfo {
        self.entries.get(id.get() as usize).unwrap_or(&DEFAULT_INFO)
    }

    /// All registered entries in id order.
    #[must_use]
    pub fn entries(&self) -> &[BlockInfo] {
        &self.entries
    }

    /// Number of registered blocks.
    #[must_use]
    pub fn len(&self) -> usize {
        self.entries.len()
    }

    /// True iff no blocks have been registered.
    #[must_use]
    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }
}

// ---------- BaseBlocks ----------

/// Ids assigned by `register_base_blocks`. Stored alongside the registry by
/// the caller; mirrors the C++ `BaseBlocks` struct.
#[derive(Copy, Clone, Debug, Default, Eq, PartialEq)]
pub struct BaseBlocks {
    pub air: Id,
    pub rock: Id,
    pub grass: Id,
    pub dirt: Id,
    pub stone: Id,
    pub plank: Id,
    pub wood: Id,
    pub bedrock: Id,
    pub leaf: Id,
    pub glass: Id,
    pub water: Id,
    pub lava: Id,
    pub glowstone: Id,
    pub sand: Id,
    pub cement: Id,
    pub ice: Id,
    pub coal: Id,
    pub iron: Id,
    pub tnt: Id,
}

/// Helper: build a state-independent `BlockInfo` with the `[top, side,
/// bottom]` face mapping required by `BlockInfo::face`.
fn info(
    name: &'static str,
    solid: bool,
    opaque: bool,
    translucent: bool,
    hardness: f32,
    faces: [TextureIndex; 3],
) -> BlockInfo {
    BlockInfo {
        name: Cow::Borrowed(name),
        solid,
        opaque,
        translucent,
        hardness,
        faces,
        face_mapping: FaceMapping::Static,
    }
}

/// Same as [`info`] but with [`FaceMapping::AxisAligned`] — used for log
/// blocks where state 0..=5 picks one of six placement orientations.
fn info_axis(
    name: &'static str,
    solid: bool,
    opaque: bool,
    translucent: bool,
    hardness: f32,
    faces: [TextureIndex; 3],
) -> BlockInfo {
    BlockInfo {
        face_mapping: FaceMapping::AxisAligned,
        ..info(name, solid, opaque, translucent, hardness, faces)
    }
}

/// Register the base-game blocks in the same order, with the same physical
/// properties, as `src/blocks.ixx::register_base_blocks`. Face textures are
/// copied from the C++ `Textures::indices` table (which the migration plan
/// folds into `BlockInfo`).
#[allow(clippy::similar_names)] // grass / glass — domain-required block names.
pub fn register_base_blocks(registry: &mut BlockRegistry) -> BaseBlocks {
    use TextureIndex as T;
    // Each entry is `(name, solid, opaque, translucent, hardness, faces)`.
    // Face order matches the C++ `Textures::indices` table verbatim.
    let air = registry.add(info("air", false, false, false, 0.0, [T::WHITE; 3]));
    let rock = registry.add(info("rock", true, true, false, 2.0, [T::ROCK; 3]));
    let grass = registry.add(info(
        "grass",
        true,
        true,
        false,
        0.3,
        [T::GRASS_TOP, T::GRASS_SIDE, T::DIRT],
    ));
    let dirt = registry.add(info("dirt", true, true, false, 0.3, [T::DIRT; 3]));
    let stone = registry.add(info("stone", true, true, false, 1.0, [T::STONE; 3]));
    let plank = registry.add(info("plank", true, true, false, 1.0, [T::PLANK; 3]));
    // Wood is axis-aligned: `state.0 / 2` selects the cap axis, so values
    // 0..=5 encode six placement orientations. `faces[0]` is the cap
    // (rings) texture; `faces[1]` is the bark texture; `faces[2]` is
    // unused under the AxisAligned mapping.
    let wood = registry.add(info_axis(
        "wood",
        true,
        true,
        false,
        2.0,
        [T::WOOD_TOP, T::WOOD_SIDE, T::WOOD_TOP],
    ));
    let bedrock = registry.add(info("bedrock", true, true, false, 10.0, [T::BEDROCK; 3]));
    let leaf = registry.add(info("leaf", true, false, false, 0.2, [T::LEAF; 3]));
    let glass = registry.add(info("glass", true, false, false, 0.2, [T::GLASS; 3]));
    let water = registry.add(info("water", false, false, true, 0.0, [T::WATER; 3]));
    let lava = registry.add(info("lava", false, false, true, 0.0, [T::LAVA; 3]));
    let glowstone = registry.add(info(
        "glow stone",
        true,
        true,
        false,
        1.0,
        [T::GLOWSTONE; 3],
    ));
    let sand = registry.add(info("sand", true, true, false, 0.2, [T::SAND; 3]));
    let cement = registry.add(info("cement", true, true, false, 3.0, [T::CEMENT; 3]));
    let ice = registry.add(info("ice", true, false, true, 0.2, [T::ICE; 3]));
    let coal = registry.add(info("coal block", true, true, false, 0.2, [T::COAL; 3]));
    let iron = registry.add(info("iron block", true, true, false, 3.0, [T::IRON; 3]));
    let tnt = registry.add(info("tnt", true, true, false, 0.2, [T::TNT; 3]));
    BaseBlocks {
        air,
        rock,
        grass,
        dirt,
        stone,
        plank,
        wood,
        bedrock,
        leaf,
        glass,
        water,
        lava,
        glowstone,
        sand,
        cement,
        ice,
        coal,
        iron,
        tnt,
    }
}

// ---------- Tests ----------

#[cfg(test)]
#[allow(clippy::float_cmp)] // exact constants from the registry table
mod tests {
    use super::*;

    // Compile-time confirmation that `BlockData` (and its constituent
    // newtypes) is `Pod`, so chunk arrays can be memcpy'd into mesh-input
    // snapshots. If any of these stop being `Pod` the type-check fails.
    const fn assert_pod<T: bytemuck::Pod>() {}
    const _: () = {
        assert_pod::<BlockData>();
        assert_pod::<Id>();
        assert_pod::<State>();
        assert_pod::<Light>();
        assert_pod::<TextureIndex>();
    };

    #[test]
    fn light_round_trips_sky_block() {
        let l = Light::new(15, 0);
        assert_eq!(l.sky(), 15);
        assert_eq!(l.block(), 0);
        let l2 = Light::new(7, 11);
        assert_eq!(l2.sky(), 7);
        assert_eq!(l2.block(), 11);
        assert_eq!(Light::SKY, Light::new(15, 0));
        assert_eq!(Light::NONE, Light::new(0, 0));
    }

    #[test]
    fn state_round_trips_through_optional() {
        assert!(State::external().is_external());
        assert_eq!(State::new(None), State::external());
        let s = State::new(Some(7));
        assert!(!s.is_external());
        assert_eq!(s.get(), Some(7));
        assert_eq!(State::default(), State(0));
    }

    #[test]
    fn registry_assigns_sequential_ids() {
        let mut r = BlockRegistry::new();
        let a = r.add(info("a", true, true, false, 1.0, [TextureIndex::WHITE; 3]));
        let b = r.add(info("b", true, true, false, 1.0, [TextureIndex::WHITE; 3]));
        let c = r.add(info("c", true, true, false, 1.0, [TextureIndex::WHITE; 3]));
        assert_eq!(a, Id(0));
        assert_eq!(b, Id(1));
        assert_eq!(c, Id(2));
        assert_eq!(r.len(), 3);
        assert!(!r.is_empty());
    }

    #[test]
    fn registry_get_returns_default_for_out_of_range() {
        let r = BlockRegistry::new();
        let fallback = r.get(Id(42));
        assert_eq!(fallback.name, "null");
        assert!(fallback.solid);
        assert!(fallback.opaque);
        assert!(!fallback.translucent);
        assert_eq!(fallback.hardness, 0.0);
        assert_eq!(fallback.faces, [TextureIndex::NULLBLOCK; 3]);
    }

    #[test]
    fn register_base_blocks_populates_19_entries() {
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        assert_eq!(r.len(), 19);
        // `rock` has the expected non-zero hardness from the C++ table.
        assert_eq!(r.get(base.rock).hardness, 2.0);
        assert_eq!(r.get(base.air).name, "air");
    }

    #[test]
    fn base_block_face_textures_match_cpp_table() {
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        // air → all WHITE (Textures::indices row 0).
        assert_eq!(r.get(base.air).face(0), TextureIndex::WHITE);
        assert_eq!(r.get(base.air).face(1), TextureIndex::WHITE);
        assert_eq!(r.get(base.air).face(2), TextureIndex::WHITE);
        // grass → face(0)=GRASS_TOP, face(1)=GRASS_SIDE, face(2)=DIRT
        // (verbatim from C++ Textures::indices row 2).
        let grass = r.get(base.grass);
        assert_eq!(grass.face(0), TextureIndex::GRASS_TOP);
        assert_eq!(grass.face(1), TextureIndex::GRASS_SIDE);
        assert_eq!(grass.face(2), TextureIndex::DIRT);
        // wood → top, side, top.
        let wood = r.get(base.wood);
        assert_eq!(wood.face(0), TextureIndex::WOOD_TOP);
        assert_eq!(wood.face(1), TextureIndex::WOOD_SIDE);
        assert_eq!(wood.face(2), TextureIndex::WOOD_TOP);
        // Out-of-range face → NULLBLOCK fallback.
        assert_eq!(grass.face(7), TextureIndex::NULLBLOCK);
    }

    #[test]
    fn face_for_static_grass_matches_face_index_layout() {
        // For a `Static` block, `face_for(face_dir, _)` ignores state and
        // returns the same `[top, side, bottom]` slot as the legacy mesher
        // helper (top = +Y, bottom = -Y, side = everything else).
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        let grass = r.get(base.grass);
        // Any state byte should produce the same answer for a Static
        // block — try 0, 5, and 99 to verify.
        for s in [State(0), State(5), State(99)] {
            assert_eq!(grass.face_for(2, s), TextureIndex::GRASS_TOP); // +Y
            assert_eq!(grass.face_for(3, s), TextureIndex::DIRT); // -Y
            for face in [0usize, 1, 4, 5] {
                assert_eq!(grass.face_for(face, s), TextureIndex::GRASS_SIDE);
            }
        }
    }

    #[test]
    fn face_for_axis_aligned_wood_picks_cap_per_state() {
        // Wood is `AxisAligned`: state 0,1 → Y; 2,3 → X; 4,5 → Z.
        // Faces parallel to the cap axis sample the cap (WOOD_TOP); the
        // four perpendicular faces sample the bark (WOOD_SIDE).
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        let wood = r.get(base.wood);

        // Y-axis (default): ±Y are caps, ±X / ±Z are bark.
        for s in [State(0), State(1)] {
            assert_eq!(wood.face_for(2, s), TextureIndex::WOOD_TOP); // +Y cap
            assert_eq!(wood.face_for(3, s), TextureIndex::WOOD_TOP); // -Y cap
            for face in [0usize, 1, 4, 5] {
                assert_eq!(wood.face_for(face, s), TextureIndex::WOOD_SIDE);
            }
        }
        // X-axis: ±X are caps.
        for s in [State(2), State(3)] {
            assert_eq!(wood.face_for(0, s), TextureIndex::WOOD_TOP);
            assert_eq!(wood.face_for(1, s), TextureIndex::WOOD_TOP);
            for face in [2usize, 3, 4, 5] {
                assert_eq!(wood.face_for(face, s), TextureIndex::WOOD_SIDE);
            }
        }
        // Z-axis: ±Z are caps.
        for s in [State(4), State(5)] {
            assert_eq!(wood.face_for(4, s), TextureIndex::WOOD_TOP);
            assert_eq!(wood.face_for(5, s), TextureIndex::WOOD_TOP);
            for face in [0usize, 1, 2, 3] {
                assert_eq!(wood.face_for(face, s), TextureIndex::WOOD_SIDE);
            }
        }
    }
}

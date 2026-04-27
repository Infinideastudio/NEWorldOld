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
    /// Number of low bits reserved for orientation under the standard
    /// [`OrientationCodec::AXIS_ALIGNED`] encoding. With 3 bits, values
    /// `0..=5` encode the six placement orientations and `6..=7` are
    /// unused (fall back to identity in
    /// [`Orientation::for_axis_aligned_index`]).
    pub const ORIENTATION_BITS: u32 = 3;

    /// Bitmask covering the orientation bits. Equal to
    /// `(1 << ORIENTATION_BITS) - 1 = 0b0000_0111`.
    pub const ORIENTATION_MASK: u8 = (1 << Self::ORIENTATION_BITS) - 1;

    /// Bitmask covering the upper "interior" bits — the 5 bits a block
    /// type may freely use for its own per-cell substate (LCM3 clock +
    /// data, redstone power level, growth stage, etc.) without
    /// disturbing orientation.
    pub const INTERIOR_MASK: u8 = !Self::ORIENTATION_MASK;

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

    /// Extract the orientation portion (lower 3 bits). For blocks
    /// using the standard [`OrientationCodec::AXIS_ALIGNED`] encoding
    /// this picks one of the six rotations in
    /// [`Orientation::for_axis_aligned_index`]; for other codecs it
    /// may be ignored or interpreted differently.
    #[must_use]
    pub const fn orientation(self) -> u8 {
        self.0 & Self::ORIENTATION_MASK
    }

    /// Replace just the orientation bits, keeping the interior bits
    /// intact. Use this when placing an axis-aligned block — derive
    /// the orientation from the clicked face normal, drop it into the
    /// lower 3 bits, and let block-specific code fill the rest.
    #[must_use]
    pub const fn with_orientation(self, orientation: u8) -> Self {
        Self((self.0 & Self::INTERIOR_MASK) | (orientation & Self::ORIENTATION_MASK))
    }

    /// Extract the interior bits (upper 5), shifted down to occupy
    /// `0..=31`. Block-specific code uses this to read its own substate
    /// (LCM3 clock + data, redstone power, etc.) without seeing the
    /// orientation.
    #[must_use]
    pub const fn interior(self) -> u8 {
        self.0 >> Self::ORIENTATION_BITS
    }

    /// Replace just the interior bits, keeping the orientation intact.
    /// `interior` is interpreted as a 5-bit value; high bits are masked
    /// off rather than panicking.
    #[must_use]
    pub const fn with_interior(self, interior: u8) -> Self {
        let max_interior = u8::MAX >> Self::ORIENTATION_BITS;
        Self(
            (self.0 & Self::ORIENTATION_MASK)
                | ((interior & max_interior) << Self::ORIENTATION_BITS),
        )
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
    pub const LCM3_OUT_PORT: TextureIndex = TextureIndex(30);
    pub const LCM3_IN_PORT: TextureIndex = TextureIndex(33);
    pub const LCM3_WIRE: TextureIndex = TextureIndex(36);
    pub const LCM3_WIRE_ON: TextureIndex = TextureIndex(39);
    pub const LCM3_FF: TextureIndex = TextureIndex(42);
    pub const LCM3_FF_ON: TextureIndex = TextureIndex(45);
    pub const LCM3_NOT: TextureIndex = TextureIndex(48);
    pub const LCM3_NOT_ON: TextureIndex = TextureIndex(51);
    pub const LCM3_AND: TextureIndex = TextureIndex(54);
    pub const LCM3_AND_ON: TextureIndex = TextureIndex(57);
    pub const LCM3_OR: TextureIndex = TextureIndex(60);
    pub const LCM3_OR_ON: TextureIndex = TextureIndex(63);
}

// ---------- OrientationCodec ----------

/// Per-block-type accessor for a cell's placement orientation. Each
/// block's stored [`State`] byte may pack orientation into different bit
/// positions (or even different sub-fields, like LCM3 circuit blocks
/// whose state is `(orientation * 2 + data) * 3 + clock`), so the
/// encoding is parameterised on the block — not hard-coded to a fixed
/// enum of layouts.
///
/// `read` decodes the cell's orientation into an [`Orientation`] for
/// face-texture / mesh-UV computation.
///
/// `write` builds a state byte for placement: given a placement
/// orientation index `0..6` (the "axis index" produced by
/// `state_from_face_normal` / equivalent) and a base state `into`,
/// return the new state with the orientation slot replaced. The `into`
/// argument lets a block-specific codec preserve any *interior* state
/// already in the byte — handy for blocks whose orientation can be
/// changed independently of their data. For pure placement, callers
/// pass [`State::default()`].
///
/// The pre-defined [`Self::STATIC`] and [`Self::AXIS_ALIGNED`] codecs
/// cover the two encodings the base-game blocks use; new block kinds
/// (LCM3 wires, gates, registers, …) declare their own codec at
/// registration time.
#[derive(Copy, Clone)]
pub struct OrientationCodec {
    /// State → orientation. Called by the chunk mesher once per visible
    /// face per cell; should be branch-light. For state-independent
    /// blocks, returns [`Orientation::IDENTITY`].
    pub read: fn(State) -> Orientation,
    /// Build a state byte for placement. `orientation_index` is in
    /// `0..6` (axis-aligned slot index, see
    /// [`Orientation::for_axis_aligned_index`]); `into` is the base
    /// state (typically [`State::default()`] at fresh placement). State-
    /// independent blocks ignore the index and return `into` unchanged.
    pub write: fn(orientation_index: u8, into: State) -> State,
    /// State → orientation index (`0..6`). Inverse of `write`'s first
    /// argument. State-independent blocks return `0`. Used by
    /// [`Self::reset_to_base`] and by future commands that need to read
    /// orientation in a codec-agnostic way without going through a full
    /// [`Orientation`] matrix.
    pub orientation_index: fn(State) -> u8,
}

impl OrientationCodec {
    /// Construct a codec from raw fn pointers.
    #[must_use]
    pub const fn new(
        read: fn(State) -> Orientation,
        write: fn(u8, State) -> State,
        orientation_index: fn(State) -> u8,
    ) -> Self {
        Self {
            read,
            write,
            orientation_index,
        }
    }

    /// Codec for state-independent blocks (cobble, dirt, leaves, …).
    /// `read` always returns identity; `write` ignores the orientation
    /// index and returns its `into` argument unchanged;
    /// `orientation_index` is always `0`.
    pub const STATIC: OrientationCodec = OrientationCodec::new(
        |_| Orientation::IDENTITY,
        |_, into| into,
        |_| 0,
    );

    /// Codec for the simplest 6-orientation layout: orientation lives in
    /// the lower 3 bits of the state byte (see
    /// [`State::ORIENTATION_BITS`]); upper 5 bits are reserved for a
    /// block's own interior state. Used by `wood` and friends.
    pub const AXIS_ALIGNED: OrientationCodec = OrientationCodec::new(
        |s| Orientation::for_axis_aligned_index(s.orientation()),
        |o, into| into.with_orientation(o),
        |s| s.orientation(),
    );

    /// Codec for LCM3 circuit blocks (`wire`, `fork`, `ff`, `not`,
    /// `and`, `or`). State packs `(orientation * 2 + data) * 3 + clock`
    /// — i.e. clock in `state % 3`, data in `(state / 3) & 1`,
    /// orientation in `state / 6`. `read` extracts the orientation slot;
    /// `write` replaces it while preserving any data + clock interior
    /// already in `into` (placement uses `State::default()` so interior
    /// starts at 0); `orientation_index` is the `state / 6` projection.
    pub const LCM3: OrientationCodec = OrientationCodec::new(
        |s| Orientation::for_axis_aligned_index(s.0 / 6),
        |orientation_index, into| {
            let interior = into.0 % 6;
            // Mask the orientation index so 6/7 don't wrap into a
            // neighbouring orientation; for_axis_aligned_index treats
            // anything `>= 6` as identity at the read side.
            let safe_o = orientation_index % 8;
            State(safe_o.wrapping_mul(6).wrapping_add(interior))
        },
        |s| s.0 / 6,
    );

    /// Build a state with the cell's current orientation preserved but
    /// every interior bit cleared — i.e.
    /// `write(orientation_index(s), State::default())`. For
    /// [`Self::LCM3`] this drops data + clock to zero while keeping the
    /// placement axis (`/lcm3-reset`'s "base state"); for
    /// [`Self::AXIS_ALIGNED`] it clears the upper 5 bits; for
    /// [`Self::STATIC`] it returns [`State::default()`].
    #[must_use]
    pub fn reset_to_base(&self, state: State) -> State {
        let idx = (self.orientation_index)(state);
        (self.write)(idx, State::default())
    }
}

impl core::fmt::Debug for OrientationCodec {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        // Function pointers don't print usefully. Show the address as a
        // best-effort fingerprint so registry diffs in tests aren't
        // completely opaque.
        f.debug_struct("OrientationCodec")
            .field("read", &(self.read as usize))
            .field("write", &(self.write as usize))
            .field("orientation_index", &(self.orientation_index as usize))
            .finish()
    }
}

impl PartialEq for OrientationCodec {
    fn eq(&self, other: &Self) -> bool {
        // Function-pointer equality is "same code address" — fine for
        // the registry's PartialEq impl, which is only used in tests.
        (self.read as usize) == (other.read as usize)
            && (self.write as usize) == (other.write as usize)
            && (self.orientation_index as usize) == (other.orientation_index as usize)
    }
}

// ---------- BlockInfo ----------

/// Per-block-type face texture function. Given the block's `faces` array,
/// a static face slot (`0 = top`, `1 = side`, `2 = bottom`) — already
/// derived from world face direction + orientation by
/// [`BlockInfo::face_for`] — and the cell's full [`State`], return the
/// final atlas texture index.
///
/// The default implementation [`default_face_texture`] is just
/// `faces[face_index]` (state ignored). LCM3 circuit blocks override
/// with [`lcm3_face_texture_with_data`] / [`lcm3_face_texture_no_data`]
/// to apply per-tick clock offsets and per-data on/off variants without
/// touching the orientation pipeline.
pub type FaceTextureFn =
    fn(faces: &[TextureIndex; 3], face_index: usize, state: State) -> TextureIndex;

/// Default [`FaceTextureFn`]: return `faces[face_index]`, falling back
/// to `NULLBLOCK` for out-of-range slots. Ignores state.
#[must_use]
pub const fn default_face_texture(
    faces: &[TextureIndex; 3],
    face_index: usize,
    _state: State,
) -> TextureIndex {
    if face_index < 3 {
        faces[face_index]
    } else {
        TextureIndex::NULLBLOCK
    }
}

/// Static, registry-owned properties of a block id. Includes the per-face
/// texture array (`faces[0..3]` = top, side, bottom), a per-block
/// [`OrientationCodec`] that decodes/encodes orientation in the cell's
/// state byte, and a per-block [`FaceTextureFn`] that resolves
/// state-dependent texture variants (data on/off, clock phase) on top of
/// the orientation lookup.
// Note: no `PartialEq` derive — `face_texture` is a raw fn pointer,
// whose `==` is unpredictable across codegen units; nothing in the tree
// compares two `BlockInfo` instances, so dropping it is the simplest
// fix.
#[derive(Clone, Debug)]
pub struct BlockInfo {
    pub name: Cow<'static, str>,
    pub solid: bool,
    pub opaque: bool,
    pub translucent: bool,
    pub hardness: f32,
    /// Per-face texture indices in `[top, side, bottom]` order. Looked
    /// up by [`Self::face_for`] after rotating the world face direction
    /// back to canonical-block space through the cell's orientation —
    /// so e.g. a state-2 wood log's `+X` world face reads
    /// `faces[face_index_static(canonical +Y)] = faces[0]` (cap). For
    /// blocks that pick texture by state (LCM3 circuits), this array
    /// stores the *baseline* (data=0, clock=0) per-slot indices and
    /// [`Self::face_texture`] applies the variants.
    pub faces: [TextureIndex; 3],
    /// How this block reads / writes orientation into its state byte.
    /// Defaults to [`OrientationCodec::STATIC`] via the `info(...)`
    /// helper; oriented blocks override at registration.
    pub orientation_codec: OrientationCodec,
    /// Per-face texture-variant resolver. Defaults to
    /// [`default_face_texture`] (= `faces[face_index]`); LCM3 blocks
    /// install a clock + data-aware variant at registration.
    pub face_texture: FaceTextureFn,
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

    /// Texture for a face *direction* (`0..6`, in the mesher's
    /// `[+X, -X, +Y, -Y, +Z, -Z]` order) given the cell's [`State`].
    ///
    /// Decodes the cell's orientation through the block's
    /// [`OrientationCodec`], rotates the world face direction back to
    /// the canonical (state-0) cube, then dispatches through the
    /// block's [`FaceTextureFn`] keyed on the static face slot. For
    /// static blocks this collapses to `faces[face_index_static(face_dir)]`
    /// (the legacy behaviour); for axis-aligned blocks the cap follows
    /// the placement axis; for LCM3 circuits the texture is further
    /// modulated by data + clock interior bits.
    #[must_use]
    pub fn face_for(&self, face_dir: usize, state: State) -> TextureIndex {
        let orientation = (self.orientation_codec.read)(state);
        let canon_face = canonical_face_id(orientation, face_dir);
        let face_index = face_index_static(canon_face);
        (self.face_texture)(&self.faces, face_index, state)
    }
}

/// Map a face direction (`0..6`, `[+X, -X, +Y, -Y, +Z, -Z]`) to a
/// `BlockInfo::faces` slot under the static layout: top = 0, side = 1,
/// bottom = 2. Mirrors the switch in C++ `_merge_face_render_chunk`.
#[inline]
#[must_use]
pub fn face_index_static(face_dir: usize) -> usize {
    match face_dir {
        2 => 0,
        3 => 2,
        _ => 1,
    }
}

/// Rotate a world-space face direction (0..6) back to the canonical
/// block frame via `orientation`, then identify the canonical face id
/// it lands on. Used by [`BlockInfo::face_for`] and by the chunk mesher.
#[inline]
#[must_use]
pub fn canonical_face_id(orientation: Orientation, face_dir: usize) -> usize {
    let world_normal = AXIS_DIRS[face_dir.min(5)];
    let canon = orientation.apply_dir_i(world_normal);
    if canon[0] > 0 {
        0
    } else if canon[0] < 0 {
        1
    } else if canon[1] > 0 {
        2
    } else if canon[1] < 0 {
        3
    } else if canon[2] > 0 {
        4
    } else {
        5
    }
}

/// Unit-axis directions per face id, `[+X, -X, +Y, -Y, +Z, -Z]`.
const AXIS_DIRS: [[i32; 3]; 6] = [
    [1, 0, 0],
    [-1, 0, 0],
    [0, 1, 0],
    [0, -1, 0],
    [0, 0, 1],
    [0, 0, -1],
];

// ---------- LCM3 face textures ----------

/// Atlas-index offset between an LCM3 texture's `off` slot and its `on`
/// slot. The atlas reserves three sequential slots per "phase variant"
/// (one per `clock % 3`), so the on/off pair is exactly one phase-block
/// apart — `LCM3_WIRE_ON.0 - LCM3_WIRE.0 == 3`, ditto for FF/NOT/AND/OR.
const LCM3_DATA_OFFSET: u16 = 3;

/// Extract `(clock, data)` from an LCM3 cell's [`State`] under the
/// `(orientation * 2 + data) * 3 + clock` encoding. Orientation is
/// resolved separately by [`OrientationCodec::LCM3`].
#[inline]
const fn lcm3_clock_data(state: State) -> (u16, u16) {
    let clock = (state.0 % 3) as u16;
    let data = ((state.0 / 3) & 1) as u16;
    (clock, data)
}

/// [`FaceTextureFn`] for LCM3 blocks **with** an on/off side variant
/// (wire / ff / not / and / or). Top reads `faces[0]` (out port);
/// bottom reads `faces[2]` (in port); side reads `faces[1]` for `data=0`
/// and `faces[1] + LCM3_DATA_OFFSET` for `data=1`. All three slots are
/// then offset by `clock` (0..2) so the texture animates with the local
/// clock phase.
#[must_use]
pub fn lcm3_face_texture_with_data(
    faces: &[TextureIndex; 3],
    face_index: usize,
    state: State,
) -> TextureIndex {
    let (clock, data) = lcm3_clock_data(state);
    let base = match face_index {
        0 => faces[0].0,
        1 => faces[1].0 + data * LCM3_DATA_OFFSET,
        2 => faces[2].0,
        _ => return TextureIndex::NULLBLOCK,
    };
    TextureIndex(base + clock)
}

/// [`FaceTextureFn`] for LCM3 blocks **without** an on/off side variant
/// — currently just `fork`, whose four side faces are all out ports.
/// Each face reads `faces[face_index]` directly, then is offset by
/// `clock` (0..2). Data bit has no visual effect on this block.
#[must_use]
pub fn lcm3_face_texture_no_data(
    faces: &[TextureIndex; 3],
    face_index: usize,
    state: State,
) -> TextureIndex {
    let (clock, _data) = lcm3_clock_data(state);
    let base = if face_index < 3 {
        faces[face_index].0
    } else {
        return TextureIndex::NULLBLOCK;
    };
    TextureIndex(base + clock)
}

// ---------- Orientation ----------

/// World↔canonical rotation for a block's stored state. Used by the chunk
/// mesher to derive per-corner UVs that rotate consistently with the
/// block: an orientation index `0..6` selects one of six axis-aligned
/// rotations of the unit cube about its centre, so a state-2 (X-axis)
/// log places its bark grain along world X, a state-4 (Z-axis) log along
/// Z, and so on.
///
/// Translating a stored [`State`] into an `Orientation` is a per-block
/// concern (different blocks may pack orientation into different bit
/// positions, like LCM3 circuit blocks). The standard
/// [`OrientationCodec::AXIS_ALIGNED`] codec extracts orientation from
/// `state.orientation()` (the lower 3 bits) and maps it via
/// [`Self::for_axis_aligned_index`]:
///
/// | index | placement axis | derivation                     |
/// |-------|----------------|--------------------------------|
/// | 0     | +Y (default)   | identity                       |
/// | 1     | -Y             | 180° around world X            |
/// | 2     | +X             | -90° around world Z            |
/// | 3     | -X             | +90° around world Z            |
/// | 4     | +Z             | +90° around world X            |
/// | 5     | -Z             | -90° around world X            |
/// | 6, 7  | (undefined)    | identity fallback              |
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

    /// Pick the `world→canonical` rotation for an axis-aligned
    /// orientation slot `0..6`. Indices `>= 6` fall back to identity so
    /// an undefined state still renders a sensible upright block.
    /// Block-specific codecs use this after extracting their
    /// orientation field (see [`OrientationCodec::AXIS_ALIGNED`] for
    /// the lower-3-bit encoding the base game uses for wood).
    #[must_use]
    pub const fn for_axis_aligned_index(orientation_index: u8) -> Orientation {
        match orientation_index {
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
    orientation_codec: OrientationCodec::STATIC,
    face_texture: default_face_texture,
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
    // LCM3 circuit blocks. Each uses [`OrientationCodec::LCM3`] (state
    // packs `(orientation * 2 + data) * 3 + clock`) and applies a
    // clock + data-aware face-texture resolver. See
    // `docs/block_updates.md` for the rewrite-system design.
    pub lcm3_wire: Id,
    pub lcm3_fork: Id,
    pub lcm3_ff: Id,
    pub lcm3_not: Id,
    pub lcm3_and: Id,
    pub lcm3_or: Id,
}

impl BaseBlocks {
    /// True iff `id` is one of the six LCM3 circuit-block ids
    /// (`lcm3_wire`, `lcm3_fork`, `lcm3_ff`, `lcm3_not`, `lcm3_and`,
    /// `lcm3_or`). Used by `/lcm3-reset`'s connected-component BFS to
    /// gate which neighbours propagate.
    #[must_use]
    pub fn is_lcm3(&self, id: Id) -> bool {
        id == self.lcm3_wire
            || id == self.lcm3_fork
            || id == self.lcm3_ff
            || id == self.lcm3_not
            || id == self.lcm3_and
            || id == self.lcm3_or
    }
}

/// Helper: build a `BlockInfo` with the `[top, side, bottom]` face array,
/// orientation codec, and face-texture resolver. Most callers use the
/// thinner [`info`] / [`info_axis`] / [`info_lcm3`] wrappers below.
fn info_with(
    name: &'static str,
    solid: bool,
    opaque: bool,
    translucent: bool,
    hardness: f32,
    faces: [TextureIndex; 3],
    orientation_codec: OrientationCodec,
    face_texture: FaceTextureFn,
) -> BlockInfo {
    BlockInfo {
        name: Cow::Borrowed(name),
        solid,
        opaque,
        translucent,
        hardness,
        faces,
        orientation_codec,
        face_texture,
    }
}

/// Helper: state-independent block. Static codec + default face-texture
/// resolver (= `faces[face_index]`).
fn info(
    name: &'static str,
    solid: bool,
    opaque: bool,
    translucent: bool,
    hardness: f32,
    faces: [TextureIndex; 3],
) -> BlockInfo {
    info_with(
        name,
        solid,
        opaque,
        translucent,
        hardness,
        faces,
        OrientationCodec::STATIC,
        default_face_texture,
    )
}

/// Helper: 6-orientation axis-aligned block (orientation in the lower 3
/// bits of state). Default face-texture resolver — texture per slot is
/// just `faces[face_index]`.
fn info_axis(
    name: &'static str,
    solid: bool,
    opaque: bool,
    translucent: bool,
    hardness: f32,
    faces: [TextureIndex; 3],
) -> BlockInfo {
    info_with(
        name,
        solid,
        opaque,
        translucent,
        hardness,
        faces,
        OrientationCodec::AXIS_ALIGNED,
        default_face_texture,
    )
}

/// Helper: LCM3 circuit block. Uses [`OrientationCodec::LCM3`] (state
/// `(orientation*2 + data)*3 + clock`) and the supplied face-texture
/// resolver — typically [`lcm3_face_texture_with_data`] for blocks with
/// on/off side variants (wire/ff/not/and/or) or
/// [`lcm3_face_texture_no_data`] for fork (sides identical to top).
fn info_lcm3(
    name: &'static str,
    hardness: f32,
    faces: [TextureIndex; 3],
    face_texture: FaceTextureFn,
) -> BlockInfo {
    info_with(
        name,
        true,  // solid
        true,  // opaque
        false, // translucent
        hardness,
        faces,
        OrientationCodec::LCM3,
        face_texture,
    )
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

    // LCM3 circuit blocks. Top = OUT_PORT, bottom = IN_PORT, sides per
    // block type. State encoding: `(orientation * 2 + data) * 3 + clock`
    // — see `docs/block_updates.md`. Hardness is uniform 1.0; behaviour
    // (the rewrite rules from the doc) is not yet wired — only the
    // visual / placement layer.
    let lcm3_top = T::LCM3_OUT_PORT;
    let lcm3_bot = T::LCM3_IN_PORT;
    let lcm3_wire = registry.add(info_lcm3(
        "lcm3 wire",
        1.0,
        [lcm3_top, T::LCM3_WIRE, lcm3_bot],
        lcm3_face_texture_with_data,
    ));
    let lcm3_fork = registry.add(info_lcm3(
        "lcm3 fork",
        1.0,
        // All four sides are out ports too — sides reuse `LCM3_OUT_PORT`.
        [lcm3_top, T::LCM3_OUT_PORT, lcm3_bot],
        lcm3_face_texture_no_data,
    ));
    let lcm3_ff = registry.add(info_lcm3(
        "lcm3 flip-flop",
        1.0,
        [lcm3_top, T::LCM3_FF, lcm3_bot],
        lcm3_face_texture_with_data,
    ));
    let lcm3_not = registry.add(info_lcm3(
        "lcm3 not",
        1.0,
        [lcm3_top, T::LCM3_NOT, lcm3_bot],
        lcm3_face_texture_with_data,
    ));
    let lcm3_and = registry.add(info_lcm3(
        "lcm3 and",
        1.0,
        [lcm3_top, T::LCM3_AND, lcm3_bot],
        lcm3_face_texture_with_data,
    ));
    let lcm3_or = registry.add(info_lcm3(
        "lcm3 or",
        1.0,
        [lcm3_top, T::LCM3_OR, lcm3_bot],
        lcm3_face_texture_with_data,
    ));

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
        lcm3_wire,
        lcm3_fork,
        lcm3_ff,
        lcm3_not,
        lcm3_and,
        lcm3_or,
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
    fn register_base_blocks_populates_all_entries() {
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        // 19 base-game blocks + 6 LCM3 circuit blocks (wire, fork, ff,
        // not, and, or) = 25.
        assert_eq!(r.len(), 25);
        // `rock` has the expected non-zero hardness from the C++ table.
        assert_eq!(r.get(base.rock).hardness, 2.0);
        assert_eq!(r.get(base.air).name, "air");
        // LCM3 ids are non-zero and distinct from each other.
        assert_ne!(base.lcm3_wire, Id(0));
        assert_ne!(base.lcm3_wire, base.lcm3_fork);
        assert_ne!(base.lcm3_and, base.lcm3_or);
    }

    #[test]
    fn lcm3_face_textures_apply_clock_and_data() {
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        let wire = r.get(base.lcm3_wire);

        // State 0: orientation 0 (Y-axis), data 0, clock 0.
        // +Y face → top OUT_PORT @ clock 0.
        assert_eq!(wire.face_for(2, State(0)), TextureIndex::LCM3_OUT_PORT);
        // -Y face → bottom IN_PORT @ clock 0.
        assert_eq!(wire.face_for(3, State(0)), TextureIndex::LCM3_IN_PORT);
        // ±X / ±Z (sides) → WIRE off @ clock 0.
        assert_eq!(wire.face_for(0, State(0)), TextureIndex::LCM3_WIRE);

        // Clock advances by adding 1 / 2 to all three slots.
        // State 1: orientation 0, data 0, clock 1.
        assert_eq!(
            wire.face_for(2, State(1)),
            TextureIndex(TextureIndex::LCM3_OUT_PORT.0 + 1)
        );
        assert_eq!(
            wire.face_for(0, State(1)),
            TextureIndex(TextureIndex::LCM3_WIRE.0 + 1)
        );
        // State 2: clock 2.
        assert_eq!(
            wire.face_for(0, State(2)),
            TextureIndex(TextureIndex::LCM3_WIRE.0 + 2)
        );

        // State 3: orientation 0, data 1, clock 0 → side flips to WIRE_ON.
        assert_eq!(wire.face_for(0, State(3)), TextureIndex::LCM3_WIRE_ON);
        // State 5: orientation 0, data 1, clock 2.
        assert_eq!(
            wire.face_for(0, State(5)),
            TextureIndex(TextureIndex::LCM3_WIRE_ON.0 + 2)
        );
        // Top / bottom never use the data variant.
        assert_eq!(
            wire.face_for(2, State(5)),
            TextureIndex(TextureIndex::LCM3_OUT_PORT.0 + 2)
        );

        // Orientation rotates the cap: state 12 → orientation 2 (X-axis).
        // Now world +X reads the top OUT_PORT and ±Y/±Z read sides.
        assert_eq!(wire.face_for(0, State(12)), TextureIndex::LCM3_OUT_PORT);
        assert_eq!(wire.face_for(1, State(12)), TextureIndex::LCM3_IN_PORT);
        assert_eq!(wire.face_for(2, State(12)), TextureIndex::LCM3_WIRE);
    }

    #[test]
    fn lcm3_fork_sides_ignore_data() {
        // Fork has no on/off variant — its four sides are out ports.
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        let fork = r.get(base.lcm3_fork);

        // data=0 vs data=1 produces the same side texture (modulo clock).
        for clock in 0..3_u8 {
            let data0 = State(clock); // orientation 0, data 0, clock = clock
            let data1 = State(3 + clock); // orientation 0, data 1, clock = clock
            assert_eq!(fork.face_for(0, data0), fork.face_for(0, data1));
            // Side reads the OUT_PORT slot directly.
            assert_eq!(
                fork.face_for(0, data0),
                TextureIndex(TextureIndex::LCM3_OUT_PORT.0 + u16::from(clock))
            );
        }
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
    fn lcm3_style_custom_codec_decodes_orientation_field() {
        // Demonstrates per-block-type extensibility: a hypothetical LCM3
        // gate packs state as `(orientation * 2 + data) * 3 + clock` —
        // orientation lives in bits "above" the data + clock fields, not
        // in the lower 3 bits like wood. The codec encapsulates that
        // encoding so `face_for` works without any changes to the mesher
        // or registry.
        const fn lcm3_read(s: State) -> Orientation {
            // Extract orientation (state / 6) and clamp to the 6 valid slots.
            Orientation::for_axis_aligned_index(s.0 / 6)
        }
        const fn lcm3_write(o: u8, into: State) -> State {
            // Preserve the data + clock low fields; replace orientation only.
            let interior = into.0 % 6;
            State((o % 8).wrapping_mul(6).wrapping_add(interior))
        }
        const fn lcm3_orientation_index(s: State) -> u8 {
            s.0 / 6
        }
        const LCM3_CODEC: OrientationCodec =
            OrientationCodec::new(lcm3_read, lcm3_write, lcm3_orientation_index);

        let mut r = BlockRegistry::new();
        let lcm3_gate = r.add(BlockInfo {
            name: Cow::Borrowed("lcm3_gate"),
            solid: true,
            opaque: true,
            translucent: false,
            hardness: 1.0,
            // Distinct top vs side vs bottom textures so the test can
            // tell which canonical face each world face mapped to.
            faces: [
                TextureIndex::WOOD_TOP,
                TextureIndex::WOOD_SIDE,
                TextureIndex::ROCK,
            ],
            orientation_codec: LCM3_CODEC,
            face_texture: default_face_texture,
        });
        let info = r.get(lcm3_gate);

        // State `4 = (0 * 2 + 1) * 3 + 1` → orientation 0 (Y-axis,
        // identity), data 1, clock 1. Texture lookup should ignore the
        // data + clock fields entirely.
        assert_eq!(info.face_for(2, State(4)), TextureIndex::WOOD_TOP);
        assert_eq!(info.face_for(3, State(4)), TextureIndex::ROCK);
        assert_eq!(info.face_for(0, State(4)), TextureIndex::WOOD_SIDE);

        // State `12 = (2 * 2 + 0) * 3 + 0` → orientation 2 (X-axis),
        // data 0, clock 0. World +X should now read the cap.
        assert_eq!(info.face_for(0, State(12)), TextureIndex::WOOD_TOP);
        assert_eq!(info.face_for(2, State(12)), TextureIndex::WOOD_SIDE);

        // Round-trip through the codec: write orientation 4 (+Z) into a
        // state that already had data 1 + clock 2 (interior = 5),
        // verify the new state still decodes data + clock correctly.
        let placed = (info.orientation_codec.write)(4, State(5));
        assert_eq!(placed.0, 4 * 6 + 5);
        // And the read-side picks +Z orientation.
        assert_eq!(info.face_for(4, placed), TextureIndex::WOOD_TOP);
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

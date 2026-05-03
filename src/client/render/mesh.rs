//! CPU-side chunk mesh builder ([D1] in `docs/rust_migration.md` §5).
//!
//! Direct port of `_merge_face_render_chunk` in
//! `src/worlds/chunk_rendering.cpp`: per-face culling **with 1-D greedy run
//! merging** along one axis per face direction. Coplanar adjacent same-id
//! same-texture faces collapse into a single tiled quad — flat surfaces in
//! particular drop from `S²` quads to `S` strips per chunk side.
//!
//! ## Conventions
//!
//! * **Face id order:** `[+X, -X, +Y, -Y, +Z, -Z]`, matching the C++
//!   `coords` / `tangents` / `bitangents` arrays in `chunk_rendering.cpp`.
//! * **Vertex winding:** counter-clockwise when viewed from *outside* the
//!   cube (front-face). Each visible face emits 6 vertices in two triangles
//!   `(c0, c1, c2)` and `(c0, c2, c3)`, where `c0..c3` are the 4 corners
//!   listed below for each face. The corner order is copied verbatim from
//!   C++ `coords[face_id]`, so triangulation matches what the C++
//!   `TRIANGLE_FAN` renderer drew.
//! * **UVs:** `c0 = (0, 0)` (top-left), `c1 = (1, 0)` (top-right),
//!   `c2 = (1, 1)` (bottom-right), `c3 = (0, 1)` (bottom-left). Same as
//!   C++ `tex_coords[face_id]`.
//! * **Atlas layer:** [`crate::blocks::BlockInfo::face`] is consulted with
//!   * `face_index = 0` for `face_id 2` (+Y / top),
//!   * `face_index = 2` for `face_id 3` (-Y / bottom),
//!   * `face_index = 1` for the four side faces.
//!
//!   This mirrors the `_merge_face_render_chunk` mapping in C++
//!   (`chunk_rendering.cpp:546`).
//! * **Layer split:** a face is appended to `output.translucent` iff the
//!   cell's `BlockInfo::translucent` flag is set; otherwise to
//!   `output.opaque`. This matches the C++ `should_render(layer)` partition
//!   — non-opaque non-translucent blocks (leaf, glass) live on the opaque
//!   list.
//! * **Face culling:** mirrors C++ `should_render_face`:
//!   * the cell must not be air,
//!   * the neighbor must not be opaque,
//!   * the neighbor must have a different id, except `leaf` which always
//!     emits faces against any non-opaque neighbor (so leaf-vs-leaf
//!     interfaces stay visible — the C++ exception in
//!     `should_render_face`).

use bytemuck::{Pod, Zeroable};
use cgmath::Vector3;

use crate::client::blocks::BlockRenderRegistry;
use crate::core::blocks::{BlockData, BlockId, BlockInfo, BlockOrientation, BlockRegistry};
use crate::core::world::Chunk;

/// Side length of a chunk in blocks. Mirrors `chunks::Chunk::SIZE` from C++.
pub const CHUNK_SIZE: usize = Chunk::SIZE;

/// Padded side length: chunk size plus a one-block border on every side.
pub const PADDED_SIZE: usize = CHUNK_SIZE + 2;

/// Total blocks in the padded buffer (`18^3 = 5832`).
pub const PADDED_VOLUME: usize = PADDED_SIZE * PADDED_SIZE * PADDED_SIZE;

/// Vertex format consumed by the chunk pipeline (see [D2]).
///
/// Layout: 12 (position) + 8 (uv) + 4 (layer) + 4 (face) + 4 (light)
/// = 32 bytes, alignment 4, no trailing padding — `Pod`-safe.
///
/// The atlas `layer` doubles as the per-fragment "material" identifier:
/// the chunk fragment shader writes it into the G-buffer's material
/// target (R16Uint) so composition can branch on texture index for
/// per-material effects (water refraction, foliage shading, etc.).
/// We don't carry a separate `material_id` field because
/// `face_for(face_id, state)` already produces the right per-face
/// texture index — exactly what composition needs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct ChunkVertex {
    /// Local-to-chunk position. One unit per block; range `0..=CHUNK_SIZE`.
    pub position: [f32; 3],
    /// `0..1` within the atlas layer's face square.
    pub uv: [f32; 2],
    /// Atlas array layer index for sampling `block_diffuse` /
    /// `block_normal`. Doubles as the texture-index "material" written
    /// into the G-buffer's material target by the fragment shader.
    pub layer: u32,
    /// Face direction id `0..6` — order: `[+X, -X, +Y, -Y, +Z, -Z]`.
    pub face: u32,
    /// Packed per-vertex smooth-light *intensities*. Bottom two bytes
    /// hold `sky_byte | (block_byte << 8)`, each in `0..=255`. Each
    /// byte is the AO-averaged 4-cell intensity for its channel —
    /// per-cell levels are mapped through inverse-square falloff
    /// **before** the 4-cell average (see [`block_level_to_intensity`]
    /// / [`sky_level_to_intensity`]), then quantized to a byte. The
    /// rasterizer interpolates the bytes linearly; the fragment
    /// shader treats them as the final intensity. Upper two bytes
    /// are reserved.
    pub light: u32,
}

/// Owned snapshot of a chunk + its 26 neighbors, copied into a single padded
/// buffer so meshing is branch-free at chunk edges. Lives long enough to be
/// shipped to a worker thread (no chunk references inside).
pub struct MeshInput {
    /// Chunk coordinate (in chunk-grid space).
    pub coord: Vector3<i32>,
    /// `PADDED_SIZE^3` block data, indexed via [`padded_index`].
    pub padded: Box<[BlockData; PADDED_VOLUME]>,
    /// Per-chunk meshing toggles, captured from the live `Config` at the
    /// moment the snapshot is taken so the worker does deterministic work
    /// against a stable view. The main thread debounces config changes by
    /// dropping all meshes and re-issuing dirty chunks
    /// (see `Game::apply_mesh_config`).
    pub options: MeshOptions,
}

/// Live meshing toggles surfaced to the user via the render-options menu.
/// Plain-`Copy` so the worker can grab a snapshot per job without locking
/// any shared state. Mirrors C++ globals `SmoothLighting` / `MergeFace` /
/// `NiceGrass` from `globals.ixx`.
#[derive(Clone, Copy, Debug)]
pub struct MeshOptions {
    /// When true, the per-vertex `light` attribute is averaged over the four
    /// blocks around each face corner (smooth interpolation, soft AO). When
    /// false, every vertex of a face just uses the in-front cell's
    /// brightness — a flat-lit look that matches the C++ "advanced
    /// rendering off" path.
    pub smooth_lighting: bool,
    /// When true, coplanar same-id same-tex same-light blocks merge into
    /// strips (1-D greedy mesher). When false every visible face emits its
    /// own quad — useful for visualization / shader debugging.
    pub merge_face: bool,
    /// When true, side faces of a `grass` block sitting on top of another
    /// `grass` block use the grass-top texture instead of the side texture
    /// — the "fancy grass" look from C++ `NiceGrass`. Requires the grass
    /// id (resolved at the call site, since the registry alone doesn't
    /// expose `BaseBlocks`).
    pub nice_grass: bool,
    /// `BaseBlocks::grass`. Sentinel-zero means "nice grass disabled even
    /// if the flag is on" — the registry stripped down for tests doesn't
    /// have grass; this stays out of the way.
    pub grass_id: BlockId,
}

impl Default for MeshOptions {
    fn default() -> Self {
        Self {
            smooth_lighting: true,
            merge_face: true,
            nice_grass: true,
            grass_id: BlockId::default(),
        }
    }
}

/// Result of [`mesh_chunk`]. Tagged with the `coord` it was meshed for so the
/// main thread can re-resolve the chunk through `World::is_loaded` and skip
/// uploading a mesh whose chunk was unloaded mid-flight.
pub struct MeshOutput {
    pub coord: Vector3<i32>,
    pub opaque: Vec<ChunkVertex>,
    pub translucent: Vec<ChunkVertex>,
}

/// Index into the `PADDED_SIZE^3` padded buffer. Padded coords are `0..18`,
/// so an in-chunk cell `(x, y, z)` (each in `0..16`) maps to padded
/// `(x + 1, y + 1, z + 1)`.
pub fn padded_index(x: usize, y: usize, z: usize) -> usize {
    (z * PADDED_SIZE + y) * PADDED_SIZE + x
}

/// The four corners of each face, listed in CCW order when viewed from
/// outside the cube. Corner 0 is the conceptual "top-left" (UV (0,0)),
/// corner 1 "top-right" (UV (1,0)), corner 2 "bottom-right" (UV (1,1)),
/// corner 3 "bottom-left" (UV (0,1)). Copied verbatim from
/// `chunk_rendering.cpp` `coords[]`.
const FACE_CORNERS: [[[f32; 3]; 4]; 6] = [
    // +X (face_id 0, "Right")
    [
        [1.0, 0.0, 1.0],
        [1.0, 0.0, 0.0],
        [1.0, 1.0, 0.0],
        [1.0, 1.0, 1.0],
    ],
    // -X (face_id 1, "Left")
    [
        [0.0, 0.0, 0.0],
        [0.0, 0.0, 1.0],
        [0.0, 1.0, 1.0],
        [0.0, 1.0, 0.0],
    ],
    // +Y (face_id 2, "Top")
    [
        [0.0, 1.0, 1.0],
        [1.0, 1.0, 1.0],
        [1.0, 1.0, 0.0],
        [0.0, 1.0, 0.0],
    ],
    // -Y (face_id 3, "Bottom")
    [
        [0.0, 0.0, 0.0],
        [1.0, 0.0, 0.0],
        [1.0, 0.0, 1.0],
        [0.0, 0.0, 1.0],
    ],
    // +Z (face_id 4, "Front")
    [
        [0.0, 0.0, 1.0],
        [1.0, 0.0, 1.0],
        [1.0, 1.0, 1.0],
        [0.0, 1.0, 1.0],
    ],
    // -Z (face_id 5, "Back")
    [
        [1.0, 0.0, 0.0],
        [0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0],
        [1.0, 1.0, 0.0],
    ],
];

/// Per-corner world-space offset to apply when extending a 1-D merged quad
/// by `length` blocks along the merge axis. Mirrors C++ `coords_extend` —
/// merge-axis is `+Z` for face dirs `0..=3`, `+Y` for `4..=5`. The two
/// "outer" corners (those already at the high end of the merge axis)
/// shift, the other two stay put.
///
/// Per-corner UV deltas are derived from these offsets at run-construction
/// time via [`corner_uv_extends`], which transforms the world-extent
/// direction back to canonical-block space through [`Orientation`] before
/// projecting onto the canonical face's UV basis. That collapses to the
/// legacy `FACE_EXTEND_UV` for state-0 / static blocks but rotates with
/// the placement axis for axis-aligned blocks (logs).
const FACE_EXTEND_POS: [[[f32; 3]; 4]; 6] = [
    // +X — extend c0 & c3 along +Z
    [
        [0.0, 0.0, 1.0],
        [0.0, 0.0, 0.0],
        [0.0, 0.0, 0.0],
        [0.0, 0.0, 1.0],
    ],
    // -X — extend c1 & c2 along +Z
    [
        [0.0, 0.0, 0.0],
        [0.0, 0.0, 1.0],
        [0.0, 0.0, 1.0],
        [0.0, 0.0, 0.0],
    ],
    // +Y — extend c0 & c1 along +Z
    [
        [0.0, 0.0, 1.0],
        [0.0, 0.0, 1.0],
        [0.0, 0.0, 0.0],
        [0.0, 0.0, 0.0],
    ],
    // -Y — extend c2 & c3 along +Z
    [
        [0.0, 0.0, 0.0],
        [0.0, 0.0, 0.0],
        [0.0, 0.0, 1.0],
        [0.0, 0.0, 1.0],
    ],
    // +Z — extend c2 & c3 along +Y
    [
        [0.0, 0.0, 0.0],
        [0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0],
        [0.0, 1.0, 0.0],
    ],
    // -Z — extend c2 & c3 along +Y
    [
        [0.0, 0.0, 0.0],
        [0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0],
        [0.0, 1.0, 0.0],
    ],
];

/// Per-face neighbor offset (in padded-buffer coordinate space). Index by
/// `face_id`. Same order as `FACE_CORNERS`.
const FACE_OFFSETS: [[i32; 3]; 6] = [
    [1, 0, 0],  // +X
    [-1, 0, 0], // -X
    [0, 1, 0],  // +Y
    [0, -1, 0], // -Y
    [0, 0, 1],  // +Z
    [0, 0, -1], // -Z
];

/// Map a unit-axis integer direction to a face id `0..6`. Used to find
/// the canonical face id after rotating a world face normal back to the
/// block's local frame via [`Orientation::apply_dir_i`].
fn face_id_from_dir_i(d: [i32; 3]) -> usize {
    if d[0] > 0 {
        0
    } else if d[0] < 0 {
        1
    } else if d[1] > 0 {
        2
    } else if d[1] < 0 {
        3
    } else if d[2] > 0 {
        4
    } else {
        5
    }
}

/// Project a unit-cube point onto the canonical face's UV plane. The face
/// UV axes match the layout the legacy `FACE_UVS` table set up for
/// state-0 blocks: `+Y` → `(x, z)`, `-Y` → `(x, 1-z)`, `+X` → `(1-z, 1-y)`,
/// `-X` → `(z, 1-y)`, `+Z` → `(x, 1-y)`, `-Z` → `(1-x, 1-y)`. With this
/// projection, identity-orientation state-0 blocks round-trip to the
/// existing `FACE_UVS` corners exactly.
fn project_canon_face_point(face_id: usize, p: [f32; 3]) -> [f32; 2] {
    match face_id {
        0 => [1.0 - p[2], 1.0 - p[1]], // +X
        1 => [p[2], 1.0 - p[1]],       // -X
        2 => [p[0], p[2]],             // +Y
        3 => [p[0], 1.0 - p[2]],       // -Y
        4 => [p[0], 1.0 - p[1]],       // +Z
        5 => [1.0 - p[0], 1.0 - p[1]], // -Z
        _ => [0.0, 0.0],
    }
}

/// Linear part of [`project_canon_face_point`]: a unit step in canonical
/// XYZ shifts the UV by this much. Used for the per-corner extension UV
/// when greedy-merging — a world extension direction transforms back to
/// a canonical extension via [`Orientation::apply_dir`], then projects
/// here onto the canonical face's UV basis.
fn project_canon_face_dir(face_id: usize, d: [f32; 3]) -> [f32; 2] {
    match face_id {
        0 => [-d[2], -d[1]], // +X
        1 => [d[2], -d[1]],  // -X
        2 => [d[0], d[2]],   // +Y
        3 => [d[0], -d[2]],  // -Y
        4 => [d[0], -d[1]],  // +Z
        5 => [-d[0], -d[1]], // -Z
        _ => [0.0, 0.0],
    }
}

/// Per-corner base UV for one face direction under the given world-block
/// orientation. Each world corner is rotated back to its canonical
/// position and projected onto the canonical face's UV plane. For
/// identity orientation this matches the legacy `FACE_UVS` table; for
/// axis-aligned blocks it rotates the per-block art to follow the
/// placement axis (so e.g. a state-2 log shows its bark grain along
/// world X instead of world Y).
fn corner_uvs(face_id: usize, canon_face: usize, orientation: &BlockOrientation) -> [[f32; 2]; 4] {
    let mut out = [[0.0_f32; 2]; 4];
    for c in 0..4 {
        let p_canon = orientation.apply_point(FACE_CORNERS[face_id][c]);
        out[c] = project_canon_face_point(canon_face, p_canon);
    }
    out
}

/// Per-corner UV-extension delta along the merge axis under the given
/// orientation. Replaces the legacy `FACE_EXTEND_UV` table. Corners with
/// zero `FACE_EXTEND_POS` get zero UV delta — the canonical-direction
/// projection of `(0,0,0)` is `(0,0)`, but we short-circuit so floating-
/// point noise doesn't leak in.
fn corner_uv_extends(
    face_id: usize,
    canon_face: usize,
    orientation: &BlockOrientation,
) -> [[f32; 2]; 4] {
    let mut out = [[0.0_f32; 2]; 4];
    for c in 0..4 {
        let world_ext = FACE_EXTEND_POS[face_id][c];
        if world_ext == [0.0, 0.0, 0.0] {
            continue;
        }
        let canon_ext = orientation.apply_dir(world_ext);
        out[c] = project_canon_face_dir(canon_face, canon_ext);
    }
    out
}

/// Identify the `leaf` block id by name. The C++ `should_render_face` has a
/// hard-coded `_id != base_blocks().leaf` exception that lets leaf-vs-leaf
/// faces still emit; we look it up by namespaced internal name so this
/// routine doesn't need a `BaseBlocks` parameter (and returns `None` for
/// stripped-down test registries that haven't registered leaf).
fn find_leaf_id(registry: &BlockRegistry) -> Option<BlockId> {
    registry.id_of("neworld.leaf")
}

/// Per-corner sign on the two perpendicular axes of each face direction.
/// Used to address the 4 AO sample blocks around the in-front cell. Indexed
/// `[face][corner]` as `(sign_perp_a, sign_perp_b)`. Perp axes are: face
/// dirs `0..2` → `(Y, Z)`, `2..4` → `(X, Z)`, `4..6` → `(X, Y)`.
const CORNER_PERP_SIGNS: [[(i32, i32); 4]; 6] = [
    // +X
    [(-1, 1), (-1, -1), (1, -1), (1, 1)],
    // -X
    [(-1, -1), (-1, 1), (1, 1), (1, -1)],
    // +Y
    [(-1, 1), (1, 1), (1, -1), (-1, -1)],
    // -Y
    [(-1, -1), (1, -1), (1, 1), (-1, 1)],
    // +Z
    [(-1, -1), (1, -1), (1, 1), (-1, 1)],
    // -Z
    [(1, -1), (-1, -1), (-1, 1), (1, 1)],
];

/// `(perp_a_axis_index, perp_b_axis_index)` for each face direction. Used
/// together with [`CORNER_PERP_SIGNS`] to step from the in-front cell to
/// each AO sample cell. 0 = X, 1 = Y, 2 = Z.
const FACE_PERP_AXES: [(usize, usize); 6] = [
    (1, 2), // +X
    (1, 2), // -X
    (0, 2), // +Y
    (0, 2), // -Y
    (0, 1), // +Z
    (0, 1), // -Z
];

/// Constant factor `K` in the per-cell block-light inverse-square
/// mapping `intensity = min(1, K / (16 - level)²)`. Tweakable knob —
/// raising it brightens far-from-source blocks (longer reach), lowering
/// it tightens the falloff near sources.
const BLOCK_LIGHT_INTENSITY_K: f32 = 16.0;

/// Map a raw block-light level (`0..=15`, treated as inverse distance
/// to a source) to a normalized intensity in `[0, 1]` via
/// inverse-square falloff. Applied **per cell** during meshing,
/// before the smooth-lighting 4-cell average — averaging intensities
/// (rather than levels) avoids the "average a level then crush
/// through inverse-square" trap that turned soft AO bites into
/// near-black holes.
fn block_level_to_intensity(level: u8) -> f32 {
    let dist = 16.0 - f32::from(level);
    (BLOCK_LIGHT_INTENSITY_K / (dist * dist)).min(1.0)
}

/// Map a raw sky-light level (`0..=15`) to a normalized intensity in
/// `[0, 1]` linearly. Sky light is treated as a uniform overhead
/// illuminance rather than a point source, so the inverse-square
/// curve doesn't apply — a cell's sky level is the fraction of the
/// hemisphere that reaches it, which is already a linear quantity.
fn sky_level_to_intensity(level: u8) -> f32 {
    f32::from(level) / 15.0
}

/// Per-cell `(sky_intensity, block_intensity)` for the smooth-lighting
/// tap, both in `[0, 1]`. Opaque blocks contribute `(0, 0)` so they
/// don't drag light through solid geometry — they're always the dark
/// cell in the 4-tap average around an exposed face corner.
fn cell_intensities(block: BlockData, info: &BlockInfo) -> (f32, f32) {
    if info.opaque {
        return (0.0, 0.0);
    }
    (
        sky_level_to_intensity(block.light.sky()),
        block_level_to_intensity(block.light.block()),
    )
}

/// Read a padded cell by signed offsets. `pcx, pcy, pcz` are the *padded*
/// indices of the cell of interest (`1..=16` for in-chunk blocks); the
/// `dx/dy/dz` step is applied directly. Total result must land in `0..18`
/// — the AO sampling pattern guarantees this for chunk-interior blocks.
fn padded_at(padded: &[BlockData; PADDED_VOLUME], pcx: i32, pcy: i32, pcz: i32) -> BlockData {
    debug_assert!(
        (0..PADDED_SIZE as i32).contains(&pcx)
            && (0..PADDED_SIZE as i32).contains(&pcy)
            && (0..PADDED_SIZE as i32).contains(&pcz),
        "padded_at out of bounds: ({pcx}, {pcy}, {pcz})"
    );
    padded[padded_index(pcx as usize, pcy as usize, pcz as usize)]
}

/// Quantize a `[0, 1]` intensity to a `u8` for packing into the
/// vertex `light` field. Rounds half-up — `(intensity * 255).round()`.
fn intensity_to_byte(intensity: f32) -> u8 {
    (intensity.clamp(0.0, 1.0) * 255.0 + 0.5) as u8
}

/// Pack two intensity bytes into one `u16` for the vertex pipeline.
/// Bottom byte = sky, top byte = block (matches the unpack in
/// `chunk.wgsl::vs_main`).
const fn pack_light_bytes(sky: u8, block: u8) -> u16 {
    (block as u16) << 8 | (sky as u16)
}

/// 4-corner smooth-lighting tap for the face on cell at padded coord
/// `(pcx, pcy, pcz)` facing direction `face_id`. Each corner averages
/// the sky and block light *intensities* (already mapped through
/// inverse-square per cell — see [`cell_intensities`]) of 4 blocks
/// around the in-front (face-normal-direction) neighbor — the in-front
/// itself, two perpendicular-axis neighbors, and the diagonal corner.
/// Sky and block channels are averaged independently and quantized to
/// `u8` per channel, packed into a `u16` (sky low byte, block high
/// byte). Output is one packed value per corner in the same order as
/// [`FACE_CORNERS`].
fn corner_lights(
    padded: &[BlockData; PADDED_VOLUME],
    registry: &BlockRegistry,
    pcx: i32,
    pcy: i32,
    pcz: i32,
    face_id: usize,
) -> [u16; 4] {
    let off = FACE_OFFSETS[face_id];
    let ix = pcx + off[0];
    let iy = pcy + off[1];
    let iz = pcz + off[2];
    let (axis_a, axis_b) = FACE_PERP_AXES[face_id];
    let mut step_a = [0i32; 3];
    let mut step_b = [0i32; 3];
    step_a[axis_a] = 1;
    step_b[axis_b] = 1;

    let mut out = [0u16; 4];
    for c in 0..4 {
        let (sa, sb) = CORNER_PERP_SIGNS[face_id][c];
        let a_ofs = [step_a[0] * sa, step_a[1] * sa, step_a[2] * sa];
        let b_ofs = [step_b[0] * sb, step_b[1] * sb, step_b[2] * sb];
        let blocks = [
            (ix, iy, iz),
            (ix + a_ofs[0], iy + a_ofs[1], iz + a_ofs[2]),
            (ix + b_ofs[0], iy + b_ofs[1], iz + b_ofs[2]),
            (
                ix + a_ofs[0] + b_ofs[0],
                iy + a_ofs[1] + b_ofs[1],
                iz + a_ofs[2] + b_ofs[2],
            ),
        ];
        let mut sky_sum = 0.0_f32;
        let mut block_sum = 0.0_f32;
        for (cx, cy, cz) in blocks {
            let b = padded_at(padded, cx, cy, cz);
            let (s, k) = cell_intensities(b, registry.get(b.id));
            sky_sum += s;
            block_sum += k;
        }
        let sky_byte = intensity_to_byte(sky_sum * 0.25);
        let block_byte = intensity_to_byte(block_sum * 0.25);
        out[c] = pack_light_bytes(sky_byte, block_byte);
    }
    out
}

/// In-flight 1-D greedy run accumulator: a maximal contiguous strip of
/// adjacent same-id same-tex same-light faces along one axis of one face
/// direction. Flushed to the output bucket on first mismatch (or end of
/// strip).
#[derive(Clone, Copy)]
struct Run {
    /// Chunk-local start cell of the run.
    start: [i32; 3],
    /// Face direction id (0..6).
    face: usize,
    /// Number of blocks already merged *beyond* the start cell. `length == 0`
    /// is a single-cell run.
    length: i32,
    /// Atlas layer index. Two runs merge only if they pick the same layer
    /// (so e.g. a grass strip won't merge across a `+X` `→` `+Y` direction
    /// flip — handled by separating the loops by `face` first).
    layer: u32,
    /// True when the cell is `BlockInfo::translucent`. Picks the output
    /// vertex bucket on flush.
    translucent: bool,
    /// 4-corner packed smooth-light intensities (`sky_byte |
    /// (block_byte << 8)`, one `u16` per corner). The run only
    /// extends if subsequent blocks produce *identical* per-corner
    /// values.
    ///
    /// Naively this isn't sufficient: if the blocks carried independent
    /// gradients along the merge axis, normal (per-cell) rendering
    /// would emit `N` short gradients while a merged run would emit
    /// one long gradient interpolated end-to-end, which is visibly
    /// different in the middle. We can still merge here because of an
    /// invariant of the smooth-lighting algorithm: a corner's light
    /// is the 4-cell average around the *vertex* (the corner shared
    /// between adjacent blocks), not around the cell itself. Adjacent
    /// blocks therefore necessarily agree on the brightness of their
    /// shared corner. So when corner 1 of cell A equals corner 0 of
    /// cell B AND the run-extension check (`r.lights == lights`)
    /// passes, the two blocks' shared corner bytes already match the
    /// continuation of the gradient — extending the run produces the
    /// exact same interpolated brightness at every point as `N`
    /// independent quads would have.
    lights: [u16; 4],
    /// Per-corner base UVs at run start. Run extension also gates on
    /// `base_uv == cell.base_uv`, so two blocks with different
    /// orientations only merge if they happen to produce identical UVs
    /// on this face (which means they look identical on it). For
    /// `FaceMapping::Static` blocks this is always true; for
    /// `AxisAligned` blocks it filters out orientation-mismatched logs
    /// without needing a separate state check.
    base_uv: [[f32; 2]; 4],
    /// Per-corner UV delta per unit length along the merge axis. Replaces
    /// the legacy global `FACE_EXTEND_UV` lookup; folds the per-state
    /// rotation through the canonical-face projection at run start. Also
    /// gated on for run extension — two blocks with matching `base_uv`
    /// but mismatched `extend_uv` would tile differently across the
    /// merged span, so they cannot merge.
    extend_uv: [[f32; 2]; 4],
}

/// Per-direction (i, j, k) → (x, y, z) projection. `i, j` index the plane
/// perpendicular to the merge axis; `k` walks the merge axis. Mirrors the
/// switch in `chunk_rendering.cpp::_merge_face_render_chunk`.
fn project_axes(face_id: usize, i: i32, j: i32, k: i32) -> (i32, i32, i32) {
    match face_id {
        0 | 1 => (i, j, k), // +X / -X — merge along Z
        2 | 3 => (j, i, k), // +Y / -Y — merge along Z
        _ => (j, k, i),     // +Z / -Z — merge along Y
    }
}

/// Build a CPU mesh for one chunk by 1-D greedy merging per face direction.
///
/// For each of the six face directions, walks the perpendicular plane in
/// `(i, j)` and accumulates a 1-D run of visible faces along the third axis.
/// Adjacent blocks that share an id, atlas layer, and layer-bucket
/// (opaque vs translucent) collapse into a single tiled quad — cutting
/// vertex counts on flat surfaces (terrain top, walls) by `~CHUNK_SIZE×`
/// in the best case. See the module docs for face-id, winding, UV, and
/// layer-split conventions.
///
/// `input.options` controls whether smooth lighting / greedy merging /
/// "nice grass" are applied; toggling them in the menu is meant to be
/// instant, so the caller drops all `ChunkMesh`es and re-marks every
/// chunk dirty when these change (see `Game::apply_mesh_config`).
pub fn mesh_chunk(
    input: &MeshInput,
    registry: &BlockRegistry,
    render_registry: &BlockRenderRegistry,
) -> MeshOutput {
    let leaf_id = find_leaf_id(registry);
    let opts = input.options;
    // Worst case per direction is `S²` strips of `S` quads → 6 × `S²`
    // strips. Realistic terrain emits a small fraction of that.
    let initial = 6 * CHUNK_SIZE * CHUNK_SIZE / 2;
    let mut opaque: Vec<ChunkVertex> = Vec::with_capacity(initial);
    let mut translucent: Vec<ChunkVertex> = Vec::new();

    let s = CHUNK_SIZE as i32;
    for (face_id, off) in FACE_OFFSETS.iter().enumerate() {
        for i in 0..s {
            for j in 0..s {
                let mut run: Option<Run> = None;
                for k in 0..s {
                    let (x, y, z) = project_axes(face_id, i, j, k);
                    let cell = input.padded
                        [padded_index((x + 1) as usize, (y + 1) as usize, (z + 1) as usize)];
                    if cell.id == BlockId::default() {
                        flush_run(&mut opaque, &mut translucent, run.take());
                        continue;
                    }
                    let cell_info = registry.get(cell.id);
                    let neighbor = input.padded[padded_index(
                        (x + 1 + off[0]) as usize,
                        (y + 1 + off[1]) as usize,
                        (z + 1 + off[2]) as usize,
                    )];
                    let neighbor_info = registry.get(neighbor.id);

                    // Mirror C++ `should_render_face`:
                    //   if (neighbor.opaque()) break run;
                    //   if (id == neighbor.id && id != leaf) break run;
                    if neighbor_info.opaque || (cell.id == neighbor.id && Some(cell.id) != leaf_id)
                    {
                        flush_run(&mut opaque, &mut translucent, run.take());
                        continue;
                    }

                    // "Nice grass": for the four side faces of a grass cell
                    // sitting on top of another grass cell, use the
                    // grass-top texture (face index 0) instead of the side
                    // texture (face index 1). The diagonal-down probe is
                    // `(in-front + (-Y))` — same shape as the C++
                    // `_merge_face_render_chunk` lookup.
                    //
                    // Default texture lookup is state-aware via
                    // `face_for(face_id, state)` — logs use
                    // `FaceMapping::AxisAligned` to pick cap vs bark from
                    // the state byte; everything else uses the static
                    // top/side/bottom layout.
                    let tex_index_override = (opts.nice_grass
                        && opts.grass_id != BlockId::default()
                        && cell.id == opts.grass_id
                        && (face_id == 0 || face_id == 1 || face_id == 4 || face_id == 5))
                        .then(|| {
                            let probe = input.padded[padded_index(
                                (x + 1 + off[0]) as usize,
                                (y + 1 - 1) as usize,
                                (z + 1 + off[2]) as usize,
                            )];
                            (probe.id == opts.grass_id).then_some(0usize)
                        })
                        .flatten();
                    let tex_layer = match tex_index_override {
                        Some(slot) => u32::from(render_registry.face(cell.id, slot).0),
                        None => u32::from(
                            render_registry
                                .face_for(cell.id, face_id, cell.state, cell_info)
                                .0,
                        ),
                    };

                    // Orientation-aware per-corner UVs: rotate the world
                    // corners back to canonical space, project onto the
                    // canonical face's UV plane. For static blocks this
                    // collapses to the legacy `FACE_UVS` / `FACE_EXTEND_UV`;
                    // for axis-aligned blocks (logs) the bark grain
                    // follows the placement axis through the four lateral
                    // faces and the cap rotates with the log end.
                    let orientation =
                        BlockOrientation::for_block(&cell_info.face_mapping, cell.state);
                    let canon_face =
                        face_id_from_dir_i(orientation.apply_dir_i(FACE_OFFSETS[face_id]));
                    let base_uv = corner_uvs(face_id, canon_face, &orientation);
                    let extend_uv = corner_uv_extends(face_id, canon_face, &orientation);
                    let translucent_cell = cell_info.translucent;
                    let lights = if opts.smooth_lighting {
                        corner_lights(&input.padded, registry, x + 1, y + 1, z + 1, face_id)
                    } else {
                        // Flat lighting: every corner gets the in-front
                        // cell's packed light byte, so the rasterizer
                        // interpolates a uniform value (no gradient).
                        let flat =
                            flat_face_light(&input.padded, registry, x + 1, y + 1, z + 1, face_id);
                        [flat; 4]
                    };

                    // Run extension is gated on `merge_face`: with the flag
                    // off, force a flush after every cell so each face
                    // emits its own quad. With it on, two blocks merge
                    // iff their visual output on this face is identical
                    // — same atlas layer, same per-corner light bytes,
                    // same per-corner UVs (base + extend), same
                    // translucent bucket. Block id and state are
                    // deliberately not gated on: two distinct blocks
                    // that share the same texture art (e.g. an alias
                    // block) tile correctly, and two states whose
                    // base/extend UVs collapse to the same numbers
                    // (e.g. wood states 0 and 1, with our symmetric
                    // ring texture) merge harmlessly.
                    //
                    // The `lights` equality is *sufficient* even when
                    // a face carries a smooth-lighting gradient — see
                    // the `Run::lights` doc comment for why.
                    let can_extend = opts.merge_face
                        && match run.as_ref() {
                            Some(r) => {
                                r.layer == tex_layer
                                    && r.translucent == translucent_cell
                                    && r.lights == lights
                                    && r.base_uv == base_uv
                                    && r.extend_uv == extend_uv
                            }
                            None => false,
                        };
                    if can_extend {
                        run.as_mut().expect("can_extend → run is Some").length += 1;
                    } else {
                        flush_run(&mut opaque, &mut translucent, run.take());
                        run = Some(Run {
                            start: [x, y, z],
                            face: face_id,
                            length: 0,
                            layer: tex_layer,
                            translucent: translucent_cell,
                            lights,
                            base_uv,
                            extend_uv,
                        });
                    }
                }
                flush_run(&mut opaque, &mut translucent, run.take());
            }
        }
    }

    MeshOutput {
        coord: input.coord,
        opaque,
        translucent,
    }
}

/// Single-cell packed light intensities for the face's in-front
/// neighbor — used as the flat-lighting fallback when
/// `MeshOptions::smooth_lighting` is off. Returns the same `u16`
/// packing as [`corner_lights`].
fn flat_face_light(
    padded: &[BlockData; PADDED_VOLUME],
    registry: &BlockRegistry,
    pcx: i32,
    pcy: i32,
    pcz: i32,
    face_id: usize,
) -> u16 {
    let off = FACE_OFFSETS[face_id];
    let ix = pcx + off[0];
    let iy = pcy + off[1];
    let iz = pcz + off[2];
    let in_front = padded_at(padded, ix, iy, iz);
    let (s, k) = cell_intensities(in_front, registry.get(in_front.id));
    pack_light_bytes(intensity_to_byte(s), intensity_to_byte(k))
}

/// Emit the 6 vertices of `run` (after extension along the merge axis) into
/// the matching output bucket. No-op when `run` is `None`.
fn flush_run(opaque: &mut Vec<ChunkVertex>, translucent: &mut Vec<ChunkVertex>, run: Option<Run>) {
    let Some(run) = run else {
        return;
    };
    let bucket = if run.translucent { translucent } else { opaque };
    emit_run(bucket, run);
}

/// Append the 6 vertices of one merged-quad run to `out`. Triangulation is
/// `(c0, c1, c2)` then `(c0, c2, c3)`, mirroring the implicit triangulation
/// of the C++ `TRIANGLE_FAN` renderer at four corners.
fn emit_run(out: &mut Vec<ChunkVertex>, run: Run) {
    let corners = &FACE_CORNERS[run.face];
    let extend_pos = &FACE_EXTEND_POS[run.face];
    let l = run.length as f32;
    let face_u32 = run.face as u32;
    let bx = run.start[0] as f32;
    let by = run.start[1] as f32;
    let bz = run.start[2] as f32;
    let mut v: [ChunkVertex; 4] = [ChunkVertex {
        position: [0.0; 3],
        uv: [0.0; 2],
        layer: run.layer,
        face: face_u32,
        light: 0,
    }; 4];
    for c in 0..4 {
        v[c].position = [
            bx + corners[c][0] + extend_pos[c][0] * l,
            by + corners[c][1] + extend_pos[c][1] * l,
            bz + corners[c][2] + extend_pos[c][2] * l,
        ];
        v[c].uv = [
            run.base_uv[c][0] + run.extend_uv[c][0] * l,
            run.base_uv[c][1] + run.extend_uv[c][1] * l,
        ];
        v[c].light = u32::from(run.lights[c]);
    }
    out.push(v[0]);
    out.push(v[1]);
    out.push(v[2]);
    out.push(v[0]);
    out.push(v[2]);
    out.push(v[3]);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::client::blocks::{BlockRenderRegistry, BlockTextureRegistry};
    use crate::client::game::base_blocks::register_base_block_visuals;
    use crate::core::blocks::BlockState;
    use crate::core::game::base_blocks::{BaseBlocks, register_base_blocks};

    /// Build a fresh registry populated with the base game's 19 blocks, plus
    /// the matching `BaseBlocks` ids and the client-side render registry
    /// the mesher consults for face textures.
    fn registry_with_base() -> (BlockRegistry, BlockRenderRegistry, BaseBlocks) {
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        let mut render = BlockRenderRegistry::new();
        let mut textures = BlockTextureRegistry::new();
        register_base_block_visuals(&base, &mut render, &mut textures);
        (r, render, base)
    }

    /// Build a `MeshInput` whose padded blocks are populated by the closure
    /// `f(px, py, pz)`. Padded coords run `0..PADDED_SIZE`.
    fn padded_input<F>(coord: Vector3<i32>, mut f: F) -> MeshInput
    where
        F: FnMut(usize, usize, usize) -> BlockData,
    {
        // `Box::new([_; PADDED_VOLUME])` would stack-allocate the array
        // first; instead we build a Vec and convert to a fixed-size box.
        let mut buf: Box<[BlockData; PADDED_VOLUME]> = vec![BlockData::default(); PADDED_VOLUME]
            .into_boxed_slice()
            .try_into()
            .expect("PADDED_VOLUME-sized vec");
        for z in 0..PADDED_SIZE {
            for y in 0..PADDED_SIZE {
                for x in 0..PADDED_SIZE {
                    buf[padded_index(x, y, z)] = f(x, y, z);
                }
            }
        }
        MeshInput {
            coord,
            padded: buf,
            options: MeshOptions::default(),
        }
    }

    fn block(id: BlockId) -> BlockData {
        BlockData {
            id,
            ..BlockData::default()
        }
    }

    #[test]
    fn padded_index_round_trips() {
        // Spot-check a handful of coords.
        assert_eq!(padded_index(0, 0, 0), 0);
        assert_eq!(padded_index(1, 0, 0), 1);
        assert_eq!(padded_index(0, 1, 0), PADDED_SIZE);
        assert_eq!(padded_index(0, 0, 1), PADDED_SIZE * PADDED_SIZE);
        assert_eq!(padded_index(17, 17, 17), PADDED_VOLUME - 1);
        // Ordering invariant: padded_index is strictly monotone — first in
        // x within a row, then y within a slab, then z across slabs.
        let a = padded_index(3, 5, 7);
        let b = padded_index(4, 5, 7);
        let c = padded_index(3, 6, 7);
        let d = padded_index(3, 5, 8);
        assert!(a < b);
        assert!(b < c);
        assert!(c < d);
    }

    #[test]
    fn all_air_chunk_emits_no_quads() {
        let (registry, render, _) = registry_with_base();
        let input = padded_input(Vector3::new(0, 0, 0), |_, _, _| BlockData::default());
        let output = mesh_chunk(&input, &registry, &render);
        assert_eq!(output.opaque.len() + output.translucent.len(), 0);
    }

    #[test]
    fn single_solid_block_emits_six_faces() {
        let (registry, render, base) = registry_with_base();
        // Stone block at the chunk-local center (8,8,8) → padded (9,9,9).
        // All other blocks (including the padding border) are air, so all
        // 6 faces are visible.
        let input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            if (px, py, pz) == (9, 9, 9) {
                block(base.stone)
            } else {
                block(BlockId::default())
            }
        });
        let output = mesh_chunk(&input, &registry, &render);
        assert_eq!(output.opaque.len(), 6 * 6, "6 faces × 6 verts/face");
        assert!(output.translucent.is_empty());
    }

    #[test]
    fn interior_solid_emits_no_faces() {
        let (registry, render, base) = registry_with_base();
        // Every cell of the padded buffer is stone — every interior face
        // has an opaque (and same-id) neighbor and is culled.
        let input = padded_input(Vector3::new(0, 0, 0), |_, _, _| block(base.stone));
        let output = mesh_chunk(&input, &registry, &render);
        assert_eq!(output.opaque.len(), 0);
        assert_eq!(output.translucent.len(), 0);
    }

    #[test]
    fn surface_layer_emits_top_only() {
        let (registry, render, base) = registry_with_base();
        // Dirt at chunk-local y=0 (padded y=1) for every (x,z), air above.
        // Padded y=0 (the bottom border) is also dirt — that occludes the
        // -Y faces. Padded x and z borders are dirt too so the side faces
        // see same-id (and opaque) dirt and are also culled. Padded
        // y=2..=17 is air → +Y faces are visible.
        let input = padded_input(Vector3::new(0, 0, 0), |_px, py, _pz| {
            if py <= 1 {
                block(base.dirt)
            } else {
                block(BlockId::default())
            }
        });
        let output = mesh_chunk(&input, &registry, &render);
        // 16×16 dirt top faces with greedy 1-D merging along Z (the merge
        // axis for face dir +Y) collapse to 16 strips of 16 blocks each → 16
        // quads × 6 verts = 96 vertices.
        assert_eq!(output.opaque.len(), 16 * 6);
        assert!(output.translucent.is_empty());
    }

    #[test]
    fn translucent_block_routes_to_translucent_list() {
        let (registry, render, base) = registry_with_base();
        // One water block at the center, surrounded by air. Water is
        // translucent and non-opaque.
        let input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            if (px, py, pz) == (9, 9, 9) {
                block(base.water)
            } else {
                block(BlockId::default())
            }
        });
        let output = mesh_chunk(&input, &registry, &render);
        assert_eq!(output.opaque.len(), 0);
        assert_eq!(output.translucent.len(), 6 * 6);
    }

    #[test]
    fn leaf_emits_faces_against_leaf_neighbor() {
        let (registry, render, base) = registry_with_base();
        // Two adjacent leaf blocks at (8,8,8) and (9,8,8). Leaf is the
        // C++ exception: leaf-vs-leaf interfaces *do* emit faces (so each
        // leaf block contributes 6 faces — the +X face of the left block
        // and the -X face of the right block both render, against each
        // other).
        let input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            if (px, py, pz) == (9, 9, 9) || (px, py, pz) == (10, 9, 9) {
                block(base.leaf)
            } else {
                block(BlockId::default())
            }
        });
        let output = mesh_chunk(&input, &registry, &render);
        // 2 blocks × 6 faces × 6 verts.
        assert_eq!(output.opaque.len(), 2 * 6 * 6);
        assert!(output.translucent.is_empty());
    }

    #[test]
    fn same_id_opaque_neighbor_culls_face() {
        let (registry, render, base) = registry_with_base();
        // Stone at (8,8,8) and (9,8,8). Both opaque. Each block's
        // adjoining face is culled because the neighbor is opaque (and
        // same id, but the opaque check fires first). The other 5 faces
        // of each block emit normally.
        let input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            if (px, py, pz) == (9, 9, 9) || (px, py, pz) == (10, 9, 9) {
                block(base.stone)
            } else {
                block(BlockId::default())
            }
        });
        let output = mesh_chunk(&input, &registry, &render);
        assert_eq!(output.opaque.len(), 2 * 5 * 6);
    }

    #[test]
    fn vertex_winding_top_face_is_ccw_from_outside() {
        // Sanity check the +Y winding: the first triangle (c0, c1, c2)
        // should have its normal pointing in +Y when viewed from outside.
        let c0 = FACE_CORNERS[2][0]; // (0, 1, 1)
        let c1 = FACE_CORNERS[2][1]; // (1, 1, 1)
        let c2 = FACE_CORNERS[2][2]; // (1, 1, 0)
        let e1 = [c1[0] - c0[0], c1[1] - c0[1], c1[2] - c0[2]];
        let e2 = [c2[0] - c0[0], c2[1] - c0[1], c2[2] - c0[2]];
        let n = [
            e1[1] * e2[2] - e1[2] * e2[1],
            e1[2] * e2[0] - e1[0] * e2[2],
            e1[0] * e2[1] - e1[1] * e2[0],
        ];
        assert!(
            n[1] > 0.0,
            "top face normal Y component should be positive: {n:?}"
        );
    }

    #[test]
    fn chunk_vertex_layout_is_32_bytes() {
        // The pipeline (D2) assumes 32-byte vertices with no padding —
        // 12 (pos) + 8 (uv) + 4 (layer) + 4 (face) + 4 (light). The
        // atlas `layer` doubles as the per-pixel "material" (texture
        // index) the chunk fragment shader writes into the G-buffer's
        // material attachment, so no separate material_id field is
        // needed.
        assert_eq!(core::mem::size_of::<ChunkVertex>(), 32);
        assert_eq!(core::mem::align_of::<ChunkVertex>(), 4);
    }

    #[test]
    fn greedy_merges_along_run_axis() {
        let (registry, render, base) = registry_with_base();
        // 16-cell strip of stone at chunk-local y=0, z=0 (padded y=1, z=1):
        // x=0..16 dirt-floor surrounded by air. Top (+Y) face of every cell
        // is visible. The +Y face merges along Z, so each x produces its
        // own length-1 strip — 16 quads. Verify the output has exactly 16
        // +Y quads (96 verts) and that one of them spans the full Z=0 row.
        let input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            // Bottom border (py=0) is stone too so the -Y face is culled.
            // Only the y=0 row in chunk-local coords (py=1) is solid.
            // Side borders (px=0, px=17, pz=0, pz=17) are air so the side
            // faces are exposed → those rendered as well.
            if (py == 0 || py == 1) && (1..=16).contains(&px) && (1..=16).contains(&pz) {
                block(base.stone)
            } else {
                block(BlockId::default())
            }
        });
        let output = mesh_chunk(&input, &registry, &render);
        // Count distinct face directions in the output. The +Y face strips
        // collapse from 256 blocks to 16 strips (one per x row, each
        // spanning z=0..16). The 4 side faces (±X / ±Z) along the chunk
        // edges should also collapse where possible.
        let plus_y_verts = output.opaque.iter().filter(|v| v.face == 2).count();
        // 16 strips × 6 verts = 96.
        assert_eq!(plus_y_verts, 16 * 6, "+Y faces did not greedy-merge");
    }

    #[test]
    fn face_layer_picks_grass_textures_correctly() {
        // Grass block: faces[0]=GRASS_TOP, faces[1]=GRASS_SIDE, faces[2]=DIRT.
        // Verify the meshed +Y face uses GRASS_TOP and the +X face uses
        // GRASS_SIDE.
        let (registry, render, base) = registry_with_base();
        let input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            if (px, py, pz) == (9, 9, 9) {
                block(base.grass)
            } else {
                block(BlockId::default())
            }
        });
        let output = mesh_chunk(&input, &registry, &render);
        // Find a +Y vertex and a +X vertex; check their `layer` fields.
        let want_top = render.face(base.grass, 0).0;
        let want_side = render.face(base.grass, 1).0;
        let want_bottom = render.face(base.grass, 2).0;
        let mut saw_top = false;
        let mut saw_side = false;
        let mut saw_bottom = false;
        for v in &output.opaque {
            match v.face {
                2 => {
                    assert_eq!(v.layer, u32::from(want_top));
                    saw_top = true;
                }
                3 => {
                    assert_eq!(v.layer, u32::from(want_bottom));
                    saw_bottom = true;
                }
                0 | 1 | 4 | 5 => {
                    assert_eq!(v.layer, u32::from(want_side));
                    saw_side = true;
                }
                _ => panic!("unexpected face id {}", v.face),
            }
        }
        assert!(saw_top && saw_side && saw_bottom);
    }

    /// Build a `MeshInput` with one wood block at padded coord (9,9,9), the
    /// given state, surrounded by air. Returns `(layers_by_face)` — index 0
    /// holds the layer of the +X face vertex, index 1 the -X face, etc.
    fn wood_face_layers(base: &BaseBlocks, state: BlockState) -> [u32; 6] {
        let mut input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            if (px, py, pz) == (9, 9, 9) {
                BlockData {
                    id: base.wood,
                    state,
                    ..BlockData::default()
                }
            } else {
                block(BlockId::default())
            }
        });
        // Greedy merging would still pick the same layer per face for a
        // single block, but turn it off to keep the test as close to the
        // raw face-id → layer mapping as possible.
        input.options.merge_face = false;
        let mut r = BlockRegistry::new();
        let base_ids = register_base_blocks(&mut r);
        let mut render = BlockRenderRegistry::new();
        let mut textures = BlockTextureRegistry::new();
        register_base_block_visuals(&base_ids, &mut render, &mut textures);
        let output = mesh_chunk(&input, &r, &render);
        let mut layers = [u32::MAX; 6];
        for v in &output.opaque {
            layers[v.face as usize] = v.layer;
        }
        layers
    }

    #[test]
    fn wood_state_drives_per_face_texture() {
        // Wood is `FaceMapping::AxisAligned`: state.0 / 2 selects the cap
        // axis. Faces parallel to that axis sample WOOD_TOP; the four
        // perpendicular faces sample WOOD_SIDE.
        let (_, render, base) = registry_with_base();
        let top = u32::from(render.face(base.wood, 0).0); // WOOD_TOP
        let side = u32::from(render.face(base.wood, 1).0); // WOOD_SIDE
        let _ = render; // borrow only used above

        // Y-axis (state 0 or 1).
        for s in [BlockState::inline(0), BlockState::inline(1)] {
            let l = wood_face_layers(&base, s);
            assert_eq!(l, [side, side, top, top, side, side], "state {:?}", s);
        }
        // X-axis (state 2 or 3).
        for s in [BlockState::inline(2), BlockState::inline(3)] {
            let l = wood_face_layers(&base, s);
            assert_eq!(l, [top, top, side, side, side, side], "state {:?}", s);
        }
        // Z-axis (state 4 or 5).
        for s in [BlockState::inline(4), BlockState::inline(5)] {
            let l = wood_face_layers(&base, s);
            assert_eq!(l, [side, side, side, side, top, top], "state {:?}", s);
        }
    }

    /// Read the four corner UVs of the unique quad meshed for `face_id`
    /// out of a single-cell mesh output. Asserts that exactly one quad
    /// (6 vertices) was emitted for that face.
    fn face_quad_uvs(out: &MeshOutput, face_id: u32) -> [[f32; 2]; 4] {
        let verts: Vec<&ChunkVertex> = out.opaque.iter().filter(|v| v.face == face_id).collect();
        assert_eq!(
            verts.len(),
            6,
            "expected 6 vertices for face {face_id}, got {}",
            verts.len()
        );
        // The triangulation is `(c0, c1, c2)` then `(c0, c2, c3)`, so the
        // 6 emitted vertex UVs are c0, c1, c2, c0, c2, c3. We extract
        // c0..c3 from positions 0, 1, 2, 5.
        [verts[0].uv, verts[1].uv, verts[2].uv, verts[5].uv]
    }

    fn solo_wood_mesh(
        base: &BaseBlocks,
        registry: &BlockRegistry,
        render: &BlockRenderRegistry,
        state: BlockState,
    ) -> MeshOutput {
        let mut input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            if (px, py, pz) == (9, 9, 9) {
                BlockData {
                    id: base.wood,
                    state,
                    ..BlockData::default()
                }
            } else {
                block(BlockId::default())
            }
        });
        input.options.merge_face = false;
        mesh_chunk(&input, registry, render)
    }

    #[test]
    fn state_0_wood_uvs_match_legacy_face_layout() {
        // State 0 / Static must round-trip to the same per-corner UVs the
        // pre-orientation code produced from the legacy `FACE_UVS` table:
        // c0=(0,1), c1=(1,1), c2=(1,0), c3=(0,0). This is the regression
        // anchor that says "introducing the canonical-projection
        // pipeline doesn't shift state-0 blocks."
        let (registry, render, base) = registry_with_base();
        let out = solo_wood_mesh(&base, &registry, &render, BlockState::inline(0));
        let expected = [[0.0, 1.0], [1.0, 1.0], [1.0, 0.0], [0.0, 0.0]];
        for face in 0..6_u32 {
            let uvs = face_quad_uvs(&out, face);
            assert_eq!(
                uvs, expected,
                "state 0 face {face} UVs drifted from legacy layout"
            );
        }
    }

    #[test]
    fn axis_aligned_states_rotate_bark_to_match_log_axis() {
        // For each axis-aligned state, the four "bark" side faces are
        // exactly the faces perpendicular to the cap axis. Their UV
        // mappings must put the texture's V axis (vertical = bark grain)
        // along the log's cap axis in world space.
        //
        // For each bark face, find the pair of corners that share a U
        // coordinate but differ in V. The world-space delta between
        // those two corners is the direction the texture V axis maps to
        // — and it must lie along the log's cap axis, not along the
        // two perpendicular axes.
        let (registry, render, base) = registry_with_base();
        // (state, cap_axis_index) — 0 = X, 1 = Y, 2 = Z.
        let cases = [
            (BlockState::inline(0), 1usize),
            (BlockState::inline(1), 1),
            (BlockState::inline(2), 0),
            (BlockState::inline(3), 0),
            (BlockState::inline(4), 2),
            (BlockState::inline(5), 2),
        ];
        for (state, cap_axis) in cases {
            let out = solo_wood_mesh(&base, &registry, &render, state);
            let face_axes: [usize; 6] = [0, 0, 1, 1, 2, 2];
            for face in 0..6_u32 {
                if face_axes[face as usize] == cap_axis {
                    continue; // cap face, not bark.
                }
                let uvs = face_quad_uvs(&out, face);
                let verts: Vec<&ChunkVertex> =
                    out.opaque.iter().filter(|v| v.face == face).collect();
                let positions = [
                    verts[0].position,
                    verts[1].position,
                    verts[2].position,
                    verts[5].position,
                ];
                // Find a corner pair (a, b) with the same U and
                // different V — that's the V-axis traversal in world
                // space.
                let mut found = None;
                'outer: for a in 0..4usize {
                    for b in (a + 1)..4 {
                        if (uvs[a][0] - uvs[b][0]).abs() < 1e-4
                            && (uvs[a][1] - uvs[b][1]).abs() > 0.5
                        {
                            found = Some((a, b));
                            break 'outer;
                        }
                    }
                }
                let (a, b) = found.unwrap_or_else(|| {
                    panic!("state {state:?} face {face}: no V-axis pair found in UVs {uvs:?}")
                });
                let delta = [
                    positions[b][0] - positions[a][0],
                    positions[b][1] - positions[a][1],
                    positions[b][2] - positions[a][2],
                ];
                let abs = [delta[0].abs(), delta[1].abs(), delta[2].abs()];
                let dominant = (0..3)
                    .max_by(|x, y| abs[*x].partial_cmp(&abs[*y]).unwrap())
                    .unwrap();
                assert_eq!(
                    dominant, cap_axis,
                    "state {state:?} face {face}: V-axis traversal is along axis {dominant}, expected cap axis {cap_axis}"
                );
            }
        }
    }

    #[test]
    fn wood_runs_merge_iff_uvs_match() {
        // Run extension is gated on `(layer, base_uv, extend_uv,
        // lights, translucent)` only — `block_id` and `state` are
        // *not* checked. So two logs merge along the run axis exactly
        // when their visual output on the merged face is identical:
        //
        // * State 2 + State 2 (both +X-axis): identical → merge.
        // * State 0 + State 2 (Y-axis cap meets X-axis bark on +Y):
        //   different layer → no merge.
        // * State 0 + State 1 (both Y-axis, caps look the same with
        //   our symmetric ring texture): identical layer / UVs → merge.
        let (registry, render, base) = registry_with_base();

        let make = |state_a: BlockState, state_b: BlockState| {
            padded_input(Vector3::new(0, 0, 0), move |px, py, pz| {
                // Two blocks at (9,9,9) and (9,9,10) — adjacent along +Z,
                // which is the +Y face's merge axis.
                if py == 9 && px == 9 && pz == 9 {
                    BlockData {
                        id: base.wood,
                        state: state_a,
                        ..BlockData::default()
                    }
                } else if py == 9 && px == 9 && pz == 10 {
                    BlockData {
                        id: base.wood,
                        state: state_b,
                        ..BlockData::default()
                    }
                } else {
                    block(BlockId::default())
                }
            })
        };

        let plus_y = |out: &MeshOutput| out.opaque.iter().filter(|v| v.face == 2).count();

        // Same state → +Y faces merge into one quad (6 verts).
        assert_eq!(
            plus_y(&mesh_chunk(
                &make(BlockState::inline(2), BlockState::inline(2)),
                &registry,
                &render
            )),
            6,
            "same-state logs should merge along +Y"
        );

        // Y-axis (cap) meets X-axis (bark) → different layer, no merge.
        assert_eq!(
            plus_y(&mesh_chunk(
                &make(BlockState::inline(0), BlockState::inline(2)),
                &registry,
                &render
            )),
            12,
            "cap-vs-bark on +Y should not merge"
        );

        // Y-axis state 0 meets Y-axis state 1: same layer (cap), same
        // base/extend UVs → merge. This is the visually-identical case
        // the new predicate is meant to allow through.
        assert_eq!(
            plus_y(&mesh_chunk(
                &make(BlockState::inline(0), BlockState::inline(1)),
                &registry,
                &render
            )),
            6,
            "visually identical Y-axis logs should merge across state byte"
        );
    }

    #[test]
    fn distinct_block_ids_with_same_texture_merge() {
        // The merge predicate ignores `block_id`, so two distinct registry
        // entries that happen to share a face texture tile into one quad.
        // Core info carries the gameplay flags; client `BlockRenderInfo`
        // carries the texture indices the mesher actually compares.
        use crate::client::blocks::{BlockRenderInfo, BlockTextureIndex};
        use crate::core::blocks::BlockInfo;
        const SHARED: BlockTextureIndex = BlockTextureIndex(10);
        let mut r = BlockRegistry::new();
        let id_a = r.add(BlockInfo::new("rock_a", "Rock A").solid(true));
        let id_b = r.add(BlockInfo::new("rock_b", "Rock B").solid(true));
        let mut render = BlockRenderRegistry::new();
        render.set(id_a, BlockRenderInfo::uniform(SHARED));
        render.set(id_b, BlockRenderInfo::uniform(SHARED));
        let input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            if py == 9 && px == 9 && pz == 9 {
                BlockData {
                    id: id_a,
                    ..BlockData::default()
                }
            } else if py == 9 && px == 9 && pz == 10 {
                BlockData {
                    id: id_b,
                    ..BlockData::default()
                }
            } else {
                BlockData::default() // air
            }
        });
        let out = mesh_chunk(&input, &r, &render);
        // Both blocks are non-opaque and have different ids, so the
        // shared face between them stays visible — but it does because
        // of the `id != neighbour.id` clause in `should_render_face`.
        // The +Y faces of both blocks should merge into one quad along
        // their +Z merge axis (6 verts), since they share layer / UV /
        // lighting.
        let plus_y = out.opaque.iter().filter(|v| v.face == 2).count();
        assert_eq!(
            plus_y, 6,
            "distinct ids with identical art should merge on +Y"
        );
    }
}

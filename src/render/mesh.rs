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

use crate::blocks::{BlockData, BlockInfo, BlockRegistry, Id};

/// Side length of a chunk in blocks. Mirrors `chunks::Chunk::SIZE` from C++.
pub const CHUNK_SIZE: usize = 16;

/// Padded side length: chunk size plus a one-block border on every side.
pub const PADDED_SIZE: usize = CHUNK_SIZE + 2;

/// Total cells in the padded buffer (`18^3 = 5832`).
pub const PADDED_VOLUME: usize = PADDED_SIZE * PADDED_SIZE * PADDED_SIZE;

/// Vertex format consumed by the chunk pipeline (see [D2]).
///
/// Layout: 12 (position) + 8 (uv) + 4 (layer) + 4 (face) + 4 (light) +
/// 4 (block_id) = 36 bytes, alignment 4, no trailing padding — `Pod`-safe.
///
/// `block_id` was added for the deferred renderer (Tier 4): the chunk
/// fragment shader writes it into the G-buffer's `material` target so the
/// composition pass can special-case water reflections / leaf foliage /
/// glowstone emission per pixel.
#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
pub struct ChunkVertex {
    /// Local-to-chunk position. One unit per block; range `0..=CHUNK_SIZE`.
    pub position: [f32; 3],
    /// `0..1` within the atlas layer's face square.
    pub uv: [f32; 2],
    /// Atlas array layer index for sampling `block_diffuse` / `block_normal`.
    pub layer: u32,
    /// Face direction id `0..6` — order: `[+X, -X, +Y, -Y, +Z, -Z]`.
    pub face: u32,
    /// Per-vertex smooth-light brightness. Bottom byte is the AO-averaged
    /// `[0..255]` luminance — bilinearly interpolated by the rasterizer
    /// across the four corners of the face. Upper bytes are reserved.
    pub light: u32,
    /// Block id (0..65535). Written into the G-buffer material target so
    /// the composition pass / SSR / volumetric clouds can branch on the
    /// material at the surface point. Stored as `u32` for vertex-attribute
    /// alignment; truncated to `u16` on G-buffer write.
    pub block_id: u32,
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
    /// cells around each face corner (smooth interpolation, soft AO). When
    /// false, every vertex of a face just uses the in-front cell's
    /// brightness — a flat-lit look that matches the C++ "advanced
    /// rendering off" path.
    pub smooth_lighting: bool,
    /// When true, coplanar same-id same-tex same-light cells merge into
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
    pub grass_id: Id,
    /// `Config::advanced_render`. When true, the per-face directional
    /// dimming (`apply_face_dim`) is skipped — the deferred lighting
    /// pass derives directional shading from real sun lambert + shadow
    /// PCF, and the baked dimming would otherwise double-darken side
    /// faces. When false (basic mode), the dimming is applied so the
    /// forward chunk shader produces the iconic Minecraft look without
    /// needing any extra lighting math.
    pub advanced_render: bool,
}

impl Default for MeshOptions {
    fn default() -> Self {
        Self {
            smooth_lighting: true,
            merge_face: true,
            nice_grass: true,
            grass_id: Id(0),
            advanced_render: false,
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
#[inline]
#[must_use]
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

/// UVs for the four corners of any face, in the same order as
/// `FACE_CORNERS[*]`.
///
/// The corner ordering itself follows the C++ `coords[]` table — `c0` is the
/// face's "lower-left from outside" geometric corner. The C++ shader paired
/// `c0` with UV `(0, 0)` because OpenGL's `t = 0` is at the *bottom* of the
/// texture, so `t = 0` ended up sampling the visual bottom of each per-block
/// art square (correct for the `GRASS_SIDE` / `WOOD_SIDE` / etc. anisotropic
/// blocks where dirt sits at the bottom of the square and grass at the top).
///
/// wgpu / Vulkan / D3D12 invert that convention: `t = 0` is at the top of
/// the texture data (memory row 0), and our atlas uploader stores each
/// per-block square in PNG-natural top-to-bottom order. So we flip the V
/// component here — `c0` pairs with `(0, 1)` instead of `(0, 0)` — and the
/// "geometric bottom of side face" maps back to the visual bottom of the
/// block art.
const FACE_UVS: [[f32; 2]; 4] = [[0.0, 1.0], [1.0, 1.0], [1.0, 0.0], [0.0, 0.0]];

/// Per-corner world-space offset to apply when extending a 1-D merged quad by
/// `length` cells along the merge axis. Mirrors C++ `coords_extend` —
/// merge-axis is `+Z` for face dirs `0..=3`, `+Y` for `4..=5`. The two
/// "outer" corners (those already at the high end of the merge axis) shift,
/// the other two stay put.
const FACE_EXTEND_POS: [[[f32; 3]; 4]; 6] = [
    // +X — extend c0 & c3 along +Z
    [[0.0, 0.0, 1.0], [0.0, 0.0, 0.0], [0.0, 0.0, 0.0], [0.0, 0.0, 1.0]],
    // -X — extend c1 & c2 along +Z
    [[0.0, 0.0, 0.0], [0.0, 0.0, 1.0], [0.0, 0.0, 1.0], [0.0, 0.0, 0.0]],
    // +Y — extend c0 & c1 along +Z
    [[0.0, 0.0, 1.0], [0.0, 0.0, 1.0], [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]],
    // -Y — extend c2 & c3 along +Z
    [[0.0, 0.0, 0.0], [0.0, 0.0, 0.0], [0.0, 0.0, 1.0], [0.0, 0.0, 1.0]],
    // +Z — extend c2 & c3 along +Y
    [[0.0, 0.0, 0.0], [0.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 1.0, 0.0]],
    // -Z — extend c2 & c3 along +Y
    [[0.0, 0.0, 0.0], [0.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 1.0, 0.0]],
];

/// Per-corner UV offset for run extension, V-flipped relative to C++
/// `tex_coords_extend` so the "merge_axis-length" UV delta matches the
/// V-flipped Rust `FACE_UVS`. With sampler U/V set to [`Repeat`], integer
/// extensions tile the per-block art square `length + 1` times across the
/// merged span.
///
/// [`Repeat`]: wgpu::AddressMode::Repeat
const FACE_EXTEND_UV: [[[f32; 2]; 4]; 6] = [
    // +X — c1.u, c2.u
    [[0.0, 0.0], [1.0, 0.0], [1.0, 0.0], [0.0, 0.0]],
    // -X — c1.u, c2.u
    [[0.0, 0.0], [1.0, 0.0], [1.0, 0.0], [0.0, 0.0]],
    // +Y — c2.v, c3.v (V-flipped → -1)
    [[0.0, 0.0], [0.0, 0.0], [0.0, -1.0], [0.0, -1.0]],
    // -Y — c2.v, c3.v
    [[0.0, 0.0], [0.0, 0.0], [0.0, -1.0], [0.0, -1.0]],
    // +Z — c2.v, c3.v
    [[0.0, 0.0], [0.0, 0.0], [0.0, -1.0], [0.0, -1.0]],
    // -Z — c2.v, c3.v
    [[0.0, 0.0], [0.0, 0.0], [0.0, -1.0], [0.0, -1.0]],
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

/// Map a face id to the `BlockInfo::faces[..]` index. Mirrors the C++
/// `_merge_face_render_chunk` switch (`chunk_rendering.cpp:547`):
/// `+Y` → 0 (top), `-Y` → 2 (bottom), sides → 1.
#[inline]
fn face_to_atlas_index(face_id: usize) -> usize {
    match face_id {
        2 => 0,
        3 => 2,
        _ => 1,
    }
}

/// Identify the `leaf` block id by name. The C++ `should_render_face` has a
/// hard-coded `_id != base_blocks().leaf` exception that lets leaf-vs-leaf
/// faces still emit; we look it up by name so this routine doesn't need a
/// `BaseBlocks` parameter (and works correctly even when `leaf` isn't
/// registered, e.g. in a stripped-down test registry).
fn find_leaf_id(registry: &BlockRegistry) -> Option<Id> {
    registry
        .entries()
        .iter()
        .position(|info| info.name.as_ref() == "leaf")
        .map(|i| Id(i as u16))
}

/// `air` id. `register_base_blocks` always assigns it id 0, and an empty
/// `BlockData::default()` likewise has id 0; we hard-code that here so the
/// air check can be a single integer compare rather than a registry lookup.
const AIR_ID: Id = Id(0);

/// Per-corner sign on the two perpendicular axes of each face direction.
/// Used to address the 4 AO sample cells around the in-front cell. Indexed
/// `[face][corner]` as `(sign_perp_a, sign_perp_b)`. Perp axes are: face
/// dirs `0..2` → `(Y, Z)`, `2..4` → `(X, Z)`, `4..6` → `(X, Y)`.
const CORNER_PERP_SIGNS: [[(i32, i32); 4]; 6] = [
    // +X
    [(-1,  1), (-1, -1), ( 1, -1), ( 1,  1)],
    // -X
    [(-1, -1), (-1,  1), ( 1,  1), ( 1, -1)],
    // +Y
    [(-1,  1), ( 1,  1), ( 1, -1), (-1, -1)],
    // -Y
    [(-1, -1), ( 1, -1), ( 1,  1), (-1,  1)],
    // +Z
    [(-1, -1), ( 1, -1), ( 1,  1), (-1,  1)],
    // -Z
    [( 1, -1), (-1, -1), (-1,  1), ( 1,  1)],
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

/// Per-face directional dimming factors. Mirrors C++ basic rendering mode
/// (`if (!AdvancedRender) col = col * N / 10` blocks in
/// `chunk_rendering.cpp::_render_chunk` and `_render_primitive`):
///
/// * ±X (left/right): 5/10  → 0.5
/// * ±Y (top/bottom): 10/10 → 1.0
/// * ±Z (front/back): 2/10  → 0.2
///
/// Stored as numerator-out-of-10 so the fixed-point math in
/// [`apply_face_dim`] stays in `u32` and matches the C++ integer
/// arithmetic exactly. The advanced-mode `final.fsh` path multiplies
/// the lit color by directional radiance from the sun, so it deliberately
/// skips this baked-in dimming (col stays unscaled when `AdvancedRender`
/// is true).
const FACE_DIM_NUMER: [u32; 6] = [
    5,  // +X
    5,  // -X
    10, // +Y
    10, // -Y
    2,  // +Z
    2,  // -Z
];

/// Apply the basic-mode per-face directional dimming to a brightness
/// value in `0..=255`. Returns the dimmed brightness clamped to the same
/// range. Integer math matches the C++ `col * N / 10` formula exactly.
#[inline]
fn apply_face_dim(brightness: u8, face_id: usize) -> u8 {
    let n = FACE_DIM_NUMER[face_id];
    ((u32::from(brightness) * n) / 10).min(255) as u8
}

/// Per-cell brightness `0..=255`. Mirrors C++ `BlockRenderData::_color`:
/// opaque blocks are `0` (they shouldn't contribute to the AO average since
/// they block light); non-opaque blocks return `max(sky_brightness,
/// block_brightness)` with the C++ falloff curves baked in.
#[inline]
fn cell_color(block: BlockData, info: &BlockInfo) -> u8 {
    if info.opaque {
        return 0;
    }
    let sky = f32::from(block.light.sky());
    let blk = f32::from(block.light.block());
    // Sky: exponential falloff `255 * 0.8^(15 - sky)` (C++).
    let sl = (255.0 * 0.8_f32.powf(15.0 - sky)).clamp(0.0, 255.0) as u32;
    // Block: inverse-quadratic `255 / max(1, (16 - blk)² / 10)` (C++).
    let denom = (((16.0 - blk) * (16.0 - blk)) / 10.0).max(1.0);
    let bl = (255.0 / denom).clamp(0.0, 255.0) as u32;
    sl.max(bl).min(255) as u8
}

/// Read a padded cell by signed offsets. `pcx, pcy, pcz` are the *padded*
/// indices of the cell of interest (`1..=16` for in-chunk cells); the
/// `dx/dy/dz` step is applied directly. Total result must land in `0..18`
/// — the AO sampling pattern guarantees this for chunk-interior cells.
#[inline]
fn padded_at(
    padded: &[BlockData; PADDED_VOLUME],
    pcx: i32,
    pcy: i32,
    pcz: i32,
) -> BlockData {
    debug_assert!(
        (0..PADDED_SIZE as i32).contains(&pcx)
            && (0..PADDED_SIZE as i32).contains(&pcy)
            && (0..PADDED_SIZE as i32).contains(&pcz),
        "padded_at out of bounds: ({pcx}, {pcy}, {pcz})"
    );
    padded[padded_index(pcx as usize, pcy as usize, pcz as usize)]
}

/// 4-corner smooth-lighting tap for the face on cell at padded coord
/// `(pcx, pcy, pcz)` facing direction `face_id`. Each corner averages the
/// brightness of 4 cells around the in-front (face-normal-direction)
/// neighbor — the in-front itself, two perpendicular-axis neighbors, and
/// the diagonal corner. Output is one `u8` brightness per corner in the
/// same order as [`FACE_CORNERS`].
fn corner_lights(
    padded: &[BlockData; PADDED_VOLUME],
    registry: &BlockRegistry,
    pcx: i32,
    pcy: i32,
    pcz: i32,
    face_id: usize,
    apply_dim: bool,
) -> [u8; 4] {
    let off = FACE_OFFSETS[face_id];
    let ix = pcx + off[0];
    let iy = pcy + off[1];
    let iz = pcz + off[2];
    let (axis_a, axis_b) = FACE_PERP_AXES[face_id];
    let mut step_a = [0i32; 3];
    let mut step_b = [0i32; 3];
    step_a[axis_a] = 1;
    step_b[axis_b] = 1;

    let mut out = [0u8; 4];
    for c in 0..4 {
        let (sa, sb) = CORNER_PERP_SIGNS[face_id][c];
        let a_ofs = [step_a[0] * sa, step_a[1] * sa, step_a[2] * sa];
        let b_ofs = [step_b[0] * sb, step_b[1] * sb, step_b[2] * sb];
        let cells = [
            (ix, iy, iz),
            (ix + a_ofs[0], iy + a_ofs[1], iz + a_ofs[2]),
            (ix + b_ofs[0], iy + b_ofs[1], iz + b_ofs[2]),
            (
                ix + a_ofs[0] + b_ofs[0],
                iy + a_ofs[1] + b_ofs[1],
                iz + a_ofs[2] + b_ofs[2],
            ),
        ];
        let mut sum = 0u32;
        for (cx, cy, cz) in cells {
            let b = padded_at(padded, cx, cy, cz);
            sum += u32::from(cell_color(b, registry.get(b.id)));
        }
        // Average the 4-cell smooth-light tap, then apply the basic-mode
        // per-face dimming (matches the C++ `col / 4` followed by
        // `col * N / 10` for ±X / ±Z faces). Skipped under
        // advanced rendering — the deferred composition pass derives
        // directional shading from real sun lambert + shadow PCF, and
        // baked face dimming would double-darken side faces.
        let avg = (sum / 4) as u8;
        out[c] = if apply_dim {
            apply_face_dim(avg, face_id)
        } else {
            avg
        };
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
    /// Number of cells already merged *beyond* the start cell. `length == 0`
    /// is a single-cell run.
    length: i32,
    /// Atlas layer index. Two runs merge only if they pick the same layer
    /// (so e.g. a grass strip won't merge across a `+X` `→` `+Y` direction
    /// flip — handled by separating the loops by `face` first).
    layer: u32,
    /// Block id captured from the run's first cell. Same id is a precondition
    /// for run extension (so the merged strip's `block_id` field is
    /// well-defined per-vertex).
    block_id: u32,
    /// True when the cell is `BlockInfo::translucent`. Picks the output
    /// vertex bucket on flush.
    translucent: bool,
    /// 4-corner smooth-light brightnesses (`u8`-per-corner). The run only
    /// extends if subsequent cells produce identical lights — otherwise the
    /// rasterizer would interpolate across a discontinuity.
    lights: [u8; 4],
    /// True iff `lights[..]` aren't all equal — i.e. this cell has a real
    /// light gradient across its corners. Cells with non-uniform lighting
    /// can never extend (or be extended into); they always emit a single
    /// quad, matching the C++ `once` flag in `_merge_face_render_chunk`.
    once: bool,
}

/// Per-direction (i, j, k) → (x, y, z) projection. `i, j` index the plane
/// perpendicular to the merge axis; `k` walks the merge axis. Mirrors the
/// switch in `chunk_rendering.cpp::_merge_face_render_chunk`.
#[inline]
fn project_axes(face_id: usize, i: i32, j: i32, k: i32) -> (i32, i32, i32) {
    match face_id {
        0 | 1 => (i, j, k),     // +X / -X — merge along Z
        2 | 3 => (j, i, k),     // +Y / -Y — merge along Z
        _     => (j, k, i),     // +Z / -Z — merge along Y
    }
}

/// Build a CPU mesh for one chunk by 1-D greedy merging per face direction.
///
/// For each of the six face directions, walks the perpendicular plane in
/// `(i, j)` and accumulates a 1-D run of visible faces along the third axis.
/// Adjacent cells that share an id, atlas layer, and layer-bucket
/// (opaque vs translucent) collapse into a single tiled quad — cutting
/// vertex counts on flat surfaces (terrain top, walls) by `~CHUNK_SIZE×`
/// in the best case. See the module docs for face-id, winding, UV, and
/// layer-split conventions.
///
/// `input.options` controls whether smooth lighting / greedy merging /
/// "nice grass" are applied; toggling them in the menu is meant to be
/// instant, so the caller drops all `ChunkMesh`es and re-marks every
/// chunk dirty when these change (see `Game::apply_mesh_config`).
#[must_use]
pub fn mesh_chunk(input: &MeshInput, registry: &BlockRegistry) -> MeshOutput {
    let leaf_id = find_leaf_id(registry);
    let opts = input.options;
    // Worst case per direction is `S²` strips of `S` quads → 6 × `S²`
    // strips. Realistic terrain emits a small fraction of that.
    let initial = 6 * CHUNK_SIZE * CHUNK_SIZE / 2;
    let mut opaque: Vec<ChunkVertex> = Vec::with_capacity(initial);
    let mut translucent: Vec<ChunkVertex> = Vec::new();

    let s = CHUNK_SIZE as i32;
    for (face_id, off) in FACE_OFFSETS.iter().enumerate() {
        let layer_index = face_to_atlas_index(face_id);
        for i in 0..s {
            for j in 0..s {
                let mut run: Option<Run> = None;
                for k in 0..s {
                    let (x, y, z) = project_axes(face_id, i, j, k);
                    let cell = input.padded[padded_index(
                        (x + 1) as usize,
                        (y + 1) as usize,
                        (z + 1) as usize,
                    )];
                    if cell.id == AIR_ID {
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
                    if neighbor_info.opaque
                        || (cell.id == neighbor.id && Some(cell.id) != leaf_id)
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
                    let tex_index = if opts.nice_grass
                        && opts.grass_id != Id(0)
                        && cell.id == opts.grass_id
                        && (face_id == 0 || face_id == 1 || face_id == 4 || face_id == 5)
                    {
                        let probe = input.padded[padded_index(
                            (x + 1 + off[0]) as usize,
                            (y + 1 - 1) as usize,
                            (z + 1 + off[2]) as usize,
                        )];
                        if probe.id == opts.grass_id { 0 } else { layer_index }
                    } else {
                        layer_index
                    };

                    let tex_layer = u32::from(cell_info.face(tex_index).0);
                    let translucent_cell = cell_info.translucent;
                    let apply_dim = !opts.advanced_render;
                    let lights = if opts.smooth_lighting {
                        corner_lights(
                            &input.padded,
                            registry,
                            x + 1,
                            y + 1,
                            z + 1,
                            face_id,
                            apply_dim,
                        )
                    } else {
                        // Flat lighting: every corner gets the in-front
                        // cell's brightness, so the rasterizer interpolates
                        // a uniform value (no gradient).
                        let flat = flat_face_light(
                            &input.padded,
                            registry,
                            x + 1,
                            y + 1,
                            z + 1,
                            face_id,
                            apply_dim,
                        );
                        [flat; 4]
                    };
                    let once = lights[0] != lights[1]
                        || lights[1] != lights[2]
                        || lights[2] != lights[3];

                    // Run extension is gated on `merge_face`: with the flag
                    // off, force a flush after every cell so each face
                    // emits its own quad. With it on, all the usual
                    // extension predicates apply.
                    let cell_block_id = u32::from(cell.id.0);
                    let can_extend = opts.merge_face
                        && match run.as_ref() {
                            Some(r) => {
                                !r.once
                                    && !once
                                    && r.layer == tex_layer
                                    && r.translucent == translucent_cell
                                    && r.lights == lights
                                    && r.block_id == cell_block_id
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
                            block_id: cell_block_id,
                            translucent: translucent_cell,
                            lights,
                            once,
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

/// Single-cell brightness for the face's in-front neighbor — used as the
/// flat-lighting fallback when `MeshOptions::smooth_lighting` is off.
/// Applies the same per-face dimming as [`corner_lights`] so basic-mode
/// directional shading is consistent across smooth / flat lighting.
/// `apply_dim = false` skips the dimming for the advanced-rendering path.
fn flat_face_light(
    padded: &[BlockData; PADDED_VOLUME],
    registry: &BlockRegistry,
    pcx: i32,
    pcy: i32,
    pcz: i32,
    face_id: usize,
    apply_dim: bool,
) -> u8 {
    let off = FACE_OFFSETS[face_id];
    let ix = pcx + off[0];
    let iy = pcy + off[1];
    let iz = pcz + off[2];
    let in_front = padded_at(padded, ix, iy, iz);
    let raw = cell_color(in_front, registry.get(in_front.id));
    if apply_dim {
        apply_face_dim(raw, face_id)
    } else {
        raw
    }
}

/// Emit the 6 vertices of `run` (after extension along the merge axis) into
/// the matching output bucket. No-op when `run` is `None`.
#[inline]
fn flush_run(
    opaque: &mut Vec<ChunkVertex>,
    translucent: &mut Vec<ChunkVertex>,
    run: Option<Run>,
) {
    let Some(run) = run else {
        return;
    };
    let bucket = if run.translucent {
        translucent
    } else {
        opaque
    };
    emit_run(bucket, run);
}

/// Append the 6 vertices of one merged-quad run to `out`. Triangulation is
/// `(c0, c1, c2)` then `(c0, c2, c3)`, mirroring the implicit triangulation
/// of the C++ `TRIANGLE_FAN` renderer at four corners.
fn emit_run(out: &mut Vec<ChunkVertex>, run: Run) {
    let corners = &FACE_CORNERS[run.face];
    let extend_pos = &FACE_EXTEND_POS[run.face];
    let extend_uv = &FACE_EXTEND_UV[run.face];
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
        block_id: run.block_id,
    }; 4];
    for c in 0..4 {
        v[c].position = [
            bx + corners[c][0] + extend_pos[c][0] * l,
            by + corners[c][1] + extend_pos[c][1] * l,
            bz + corners[c][2] + extend_pos[c][2] * l,
        ];
        v[c].uv = [
            FACE_UVS[c][0] + extend_uv[c][0] * l,
            FACE_UVS[c][1] + extend_uv[c][1] * l,
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
    use crate::blocks::{BaseBlocks, register_base_blocks};

    /// Build a fresh registry populated with the base game's 19 blocks, plus
    /// the matching `BaseBlocks` ids.
    fn registry_with_base() -> (BlockRegistry, BaseBlocks) {
        let mut r = BlockRegistry::new();
        let base = register_base_blocks(&mut r);
        (r, base)
    }

    /// Build a `MeshInput` whose padded cells are populated by the closure
    /// `f(px, py, pz)`. Padded coords run `0..PADDED_SIZE`.
    fn padded_input<F>(coord: Vector3<i32>, mut f: F) -> MeshInput
    where
        F: FnMut(usize, usize, usize) -> BlockData,
    {
        // `Box::new([_; PADDED_VOLUME])` would stack-allocate the array
        // first; instead we build a Vec and convert to a fixed-size box.
        let mut buf: Box<[BlockData; PADDED_VOLUME]> =
            vec![BlockData::default(); PADDED_VOLUME]
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

    fn block(id: Id) -> BlockData {
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
        let (registry, base) = registry_with_base();
        let input = padded_input(Vector3::new(0, 0, 0), |_, _, _| block(base.air));
        let output = mesh_chunk(&input, &registry);
        assert_eq!(output.opaque.len() + output.translucent.len(), 0);
    }

    #[test]
    fn single_solid_block_emits_six_faces() {
        let (registry, base) = registry_with_base();
        // Stone block at the chunk-local center (8,8,8) → padded (9,9,9).
        // All other cells (including the padding border) are air, so all
        // 6 faces are visible.
        let input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            if (px, py, pz) == (9, 9, 9) {
                block(base.stone)
            } else {
                block(base.air)
            }
        });
        let output = mesh_chunk(&input, &registry);
        assert_eq!(output.opaque.len(), 6 * 6, "6 faces × 6 verts/face");
        assert!(output.translucent.is_empty());
    }

    #[test]
    fn interior_solid_emits_no_faces() {
        let (registry, base) = registry_with_base();
        // Every cell of the padded buffer is stone — every interior face
        // has an opaque (and same-id) neighbor and is culled.
        let input = padded_input(Vector3::new(0, 0, 0), |_, _, _| block(base.stone));
        let output = mesh_chunk(&input, &registry);
        assert_eq!(output.opaque.len(), 0);
        assert_eq!(output.translucent.len(), 0);
    }

    #[test]
    fn surface_layer_emits_top_only() {
        let (registry, base) = registry_with_base();
        // Dirt at chunk-local y=0 (padded y=1) for every (x,z), air above.
        // Padded y=0 (the bottom border) is also dirt — that occludes the
        // -Y faces. Padded x and z borders are dirt too so the side faces
        // see same-id (and opaque) dirt and are also culled. Padded
        // y=2..=17 is air → +Y faces are visible.
        let input = padded_input(Vector3::new(0, 0, 0), |_px, py, _pz| {
            if py <= 1 {
                block(base.dirt)
            } else {
                block(base.air)
            }
        });
        let output = mesh_chunk(&input, &registry);
        // 16×16 dirt top faces with greedy 1-D merging along Z (the merge
        // axis for face dir +Y) collapse to 16 strips of 16 cells each → 16
        // quads × 6 verts = 96 vertices.
        assert_eq!(output.opaque.len(), 16 * 6);
        assert!(output.translucent.is_empty());
    }

    #[test]
    fn translucent_block_routes_to_translucent_list() {
        let (registry, base) = registry_with_base();
        // One water block at the center, surrounded by air. Water is
        // translucent and non-opaque.
        let input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            if (px, py, pz) == (9, 9, 9) {
                block(base.water)
            } else {
                block(base.air)
            }
        });
        let output = mesh_chunk(&input, &registry);
        assert_eq!(output.opaque.len(), 0);
        assert_eq!(output.translucent.len(), 6 * 6);
    }

    #[test]
    fn leaf_emits_faces_against_leaf_neighbor() {
        let (registry, base) = registry_with_base();
        // Two adjacent leaf blocks at (8,8,8) and (9,8,8). Leaf is the
        // C++ exception: leaf-vs-leaf interfaces *do* emit faces (so each
        // leaf block contributes 6 faces — the +X face of the left block
        // and the -X face of the right block both render, against each
        // other).
        let input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            if (px, py, pz) == (9, 9, 9) || (px, py, pz) == (10, 9, 9) {
                block(base.leaf)
            } else {
                block(base.air)
            }
        });
        let output = mesh_chunk(&input, &registry);
        // 2 blocks × 6 faces × 6 verts.
        assert_eq!(output.opaque.len(), 2 * 6 * 6);
        assert!(output.translucent.is_empty());
    }

    #[test]
    fn same_id_opaque_neighbor_culls_face() {
        let (registry, base) = registry_with_base();
        // Stone at (8,8,8) and (9,8,8). Both opaque. Each block's
        // adjoining face is culled because the neighbor is opaque (and
        // same id, but the opaque check fires first). The other 5 faces
        // of each block emit normally.
        let input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            if (px, py, pz) == (9, 9, 9) || (px, py, pz) == (10, 9, 9) {
                block(base.stone)
            } else {
                block(base.air)
            }
        });
        let output = mesh_chunk(&input, &registry);
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
    fn chunk_vertex_layout_is_36_bytes() {
        // The pipeline (D2) assumes 36-byte vertices with no padding —
        // 12 (pos) + 8 (uv) + 4 (layer) + 4 (face) + 4 (light) + 4 (block_id).
        // The trailing block_id was added for the deferred renderer (Tier 4)
        // so the chunk fragment shader can write a per-pixel material into
        // the G-buffer.
        assert_eq!(core::mem::size_of::<ChunkVertex>(), 36);
        assert_eq!(core::mem::align_of::<ChunkVertex>(), 4);
    }

    #[test]
    fn face_to_atlas_index_matches_cpp_table() {
        // C++: face 2 (+Y) → top (faces[0]); face 3 (-Y) → bottom (faces[2]);
        // every other face → side (faces[1]).
        assert_eq!(face_to_atlas_index(0), 1); // +X
        assert_eq!(face_to_atlas_index(1), 1); // -X
        assert_eq!(face_to_atlas_index(2), 0); // +Y top
        assert_eq!(face_to_atlas_index(3), 2); // -Y bottom
        assert_eq!(face_to_atlas_index(4), 1); // +Z
        assert_eq!(face_to_atlas_index(5), 1); // -Z
    }

    #[test]
    fn greedy_merges_along_run_axis() {
        let (registry, base) = registry_with_base();
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
            if (py == 0 || py == 1)
                && (1..=16).contains(&px)
                && (1..=16).contains(&pz)
            {
                block(base.stone)
            } else {
                block(base.air)
            }
        });
        let output = mesh_chunk(&input, &registry);
        // Count distinct face directions in the output. The +Y face strips
        // collapse from 256 cells to 16 strips (one per x row, each
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
        let (registry, base) = registry_with_base();
        let input = padded_input(Vector3::new(0, 0, 0), |px, py, pz| {
            if (px, py, pz) == (9, 9, 9) {
                block(base.grass)
            } else {
                block(base.air)
            }
        });
        let output = mesh_chunk(&input, &registry);
        // Find a +Y vertex and a +X vertex; check their `layer` fields.
        let grass_info = registry.get(base.grass);
        let want_top = grass_info.face(0).0;
        let want_side = grass_info.face(1).0;
        let want_bottom = grass_info.face(2).0;
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
}

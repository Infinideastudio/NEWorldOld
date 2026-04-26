//! CPU-side chunk mesh builder ([D1] in `docs/rust_migration.md` §5).
//!
//! Ports the per-face culling half of the C++ greedy mesher in
//! `src/worlds/chunk_rendering.cpp`. Greedy merging is intentionally **not**
//! implemented here — every visible face emits its own quad. The greedy
//! merge can land later as a follow-up.
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

use crate::blocks::{BlockData, BlockRegistry, Id};

/// Side length of a chunk in blocks. Mirrors `chunks::Chunk::SIZE` from C++.
pub const CHUNK_SIZE: usize = 16;

/// Padded side length: chunk size plus a one-block border on every side.
pub const PADDED_SIZE: usize = CHUNK_SIZE + 2;

/// Total cells in the padded buffer (`18^3 = 5832`).
pub const PADDED_VOLUME: usize = PADDED_SIZE * PADDED_SIZE * PADDED_SIZE;

/// Vertex format consumed by the chunk pipeline (see [D2]).
///
/// Layout: 12 (position) + 8 (uv) + 4 (layer) + 4 (face) = 28 bytes,
/// alignment 4, no trailing padding — `Pod`-safe.
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
}

/// Owned snapshot of a chunk + its 26 neighbors, copied into a single padded
/// buffer so meshing is branch-free at chunk edges. Lives long enough to be
/// shipped to a worker thread (no chunk references inside).
pub struct MeshInput {
    /// Chunk coordinate (in chunk-grid space).
    pub coord: Vector3<i32>,
    /// `PADDED_SIZE^3` block data, indexed via [`padded_index`].
    pub padded: Box<[BlockData; PADDED_VOLUME]>,
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

/// Build a CPU mesh for one chunk by per-face culling.
///
/// Every visible face emits a single quad (6 vertices, two triangles). No
/// greedy merging — that's a future optimization. See the module docs for
/// the face-id, winding, UV, and layer-split conventions.
#[must_use]
pub fn mesh_chunk(input: &MeshInput, registry: &BlockRegistry) -> MeshOutput {
    let leaf_id = find_leaf_id(registry);
    // Heuristic: most chunks emit far fewer faces than the 6 × 16³ worst
    // case. 6 verts/face × 16³ / 4 ≈ 6144 verts is a reasonable starting
    // capacity that avoids most reallocs without overcommitting.
    let initial = 6 * CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE / 4;
    let mut opaque: Vec<ChunkVertex> = Vec::with_capacity(initial);
    let mut translucent: Vec<ChunkVertex> = Vec::new();

    for z in 0..CHUNK_SIZE {
        for y in 0..CHUNK_SIZE {
            for x in 0..CHUNK_SIZE {
                let cell = input.padded[padded_index(x + 1, y + 1, z + 1)];
                if cell.id == AIR_ID {
                    continue;
                }
                let cell_info = registry.get(cell.id);
                let is_translucent = cell_info.translucent;

                for (face_id, off) in FACE_OFFSETS.iter().enumerate() {
                    // Padded coords for the chunk interior are 1..=16; the
                    // ±1 offsets land in 0..=17, all valid padded indices.
                    let nx = (x as i32 + 1 + off[0]) as usize;
                    let ny = (y as i32 + 1 + off[1]) as usize;
                    let nz = (z as i32 + 1 + off[2]) as usize;
                    let neighbor = input.padded[padded_index(nx, ny, nz)];
                    let neighbor_info = registry.get(neighbor.id);

                    // Mirror C++ `should_render_face`:
                    //   if (neighbor.opaque()) return false;
                    //   if (id == neighbor.id && id != leaf) return false;
                    //   return true;
                    if neighbor_info.opaque {
                        continue;
                    }
                    if cell.id == neighbor.id && Some(cell.id) != leaf_id {
                        continue;
                    }

                    let layer_index = face_to_atlas_index(face_id);
                    let tex = cell_info.face(layer_index);
                    let bucket = if is_translucent {
                        &mut translucent
                    } else {
                        &mut opaque
                    };
                    emit_face(
                        bucket,
                        x as f32,
                        y as f32,
                        z as f32,
                        face_id,
                        u32::from(tex.0),
                    );
                }
            }
        }
    }

    MeshOutput {
        coord: input.coord,
        opaque,
        translucent,
    }
}

/// Append the 6 vertices of a single face quad to `out`. Triangulation is
/// `(c0, c1, c2)` then `(c0, c2, c3)`, matching the implicit triangulation
/// of the C++ `TRIANGLE_FAN` renderer when only 4 corners are emitted.
fn emit_face(out: &mut Vec<ChunkVertex>, bx: f32, by: f32, bz: f32, face_id: usize, layer: u32) {
    let corners = &FACE_CORNERS[face_id];
    let face_u32 = face_id as u32;
    let v: [ChunkVertex; 4] = [
        ChunkVertex {
            position: [bx + corners[0][0], by + corners[0][1], bz + corners[0][2]],
            uv: FACE_UVS[0],
            layer,
            face: face_u32,
        },
        ChunkVertex {
            position: [bx + corners[1][0], by + corners[1][1], bz + corners[1][2]],
            uv: FACE_UVS[1],
            layer,
            face: face_u32,
        },
        ChunkVertex {
            position: [bx + corners[2][0], by + corners[2][1], bz + corners[2][2]],
            uv: FACE_UVS[2],
            layer,
            face: face_u32,
        },
        ChunkVertex {
            position: [bx + corners[3][0], by + corners[3][1], bz + corners[3][2]],
            uv: FACE_UVS[3],
            layer,
            face: face_u32,
        },
    ];
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
        MeshInput { coord, padded: buf }
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
        // 16×16 dirt cells in chunk-local y=0, only +Y face visible
        // → 256 faces × 6 verts = 1536 vertices.
        assert_eq!(output.opaque.len(), 16 * 16 * 6);
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
    fn chunk_vertex_layout_is_28_bytes() {
        // The pipeline (D2) assumes 28-byte vertices with no padding.
        assert_eq!(core::mem::size_of::<ChunkVertex>(), 28);
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

// Deferred chunk shader.
//
// Vertex layout (stride 32 B, matches `gfx::mesh::ChunkVertex`):
//   @location(0) position : vec3<f32>  // offset  0
//   @location(1) uv       : vec2<f32>  // offset 12
//   @location(2) layer    : u32        // offset 20 — atlas layer +
//                                      // doubles as material/texture id
//   @location(3) face     : u32        // offset 24
//   @location(4) light    : u32        // offset 28 — packed sky / block
//                                      // light intensity bytes
//
// Per-chunk world origin: baked into `position` at upload time on the
// CPU (`ChunkMesh::upload` adds `coord * CHUNK_SIZE` to every vertex's
// position). Shader stays free of per-chunk uniforms.
//
// G-buffer layout (advanced; basic skips normal + material):
//   @location(0) diffuse  : vec4<f32>  → Rgba16Float
//                                       rgb = albedo (advanced) /
//                                             pre-lit color (basic),
//                                       a   = emissive intensity (opaque)
//                                             / texel α (translucent)
//   @location(1) normal   : vec2<f32>  → Rg8Unorm  (octahedral encoded)
//   @location(2) material : u32        → R16Uint   (atlas-layer index)
//
// Bind groups:
//   group 0 binding 0 : FrameUniforms (uniform buffer)
//   group 1 binding 0 : block_diffuse texture_2d_array<f32>
//   group 1 binding 1 : block_sampler sampler
//   group 1 binding 2 : block_normal  texture_2d_array<f32>
//   group 2 binding 0 : g_opaque_depth texture_depth_2d  (translucent
//                                                         pipelines only —
//                                                         shader-side
//                                                         "discard if
//                                                         behind opaque"
//                                                         test)

struct FrameUniforms {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    view_proj: mat4x4<f32>,
    inv_view_proj: mat4x4<f32>,
    shadow_view_proj: mat4x4<f32>,
    camera_pos: vec4<f32>,
    sun_dir: vec4<f32>,
    screen_size: vec2<f32>,
    time: f32,
    fog_start: f32,
    fog_end: f32,
    render_distance: f32,
    _pad_scalars: vec2<f32>,
    material_layers: vec4<u32>,
    shadow_params: vec4<f32>,
    player_coord_int: vec4<i32>,
    player_coord_mod: vec4<i32>,
    player_coord_frac: vec4<f32>,
}

@group(0) @binding(0)
var<uniform> frame: FrameUniforms;
@group(1) @binding(0)
var block_diffuse: texture_2d_array<f32>;
@group(1) @binding(1)
var block_sampler: sampler;
// Per-block normal map atlas — same layer indexing and UV layout as
// `block_diffuse`. Texel encoding: `(r, g, b) = (n + 1) * 0.5` in the
// face's tangent space, with `(0.5, 0.5, 1.0)` meaning "flat" (use
// face normal verbatim).
@group(1) @binding(2)
var block_normal: texture_2d_array<f32>;

// Opaque-depth texture, attached only to translucent pipelines. The
// translucent fragment uses it for shader-side occlusion: a fragment
// strictly behind the front-most opaque is discarded so it doesn't
// pollute the translucent G-buffer. Reversed-Z: larger value = closer.
@group(2) @binding(0)
var g_opaque_depth: texture_depth_2d;

struct VsIn {
    @location(0) position: vec3<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) layer: u32,
    @location(3) face: u32,
    @location(4) light: u32,
}

struct VsOut {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) @interpolate(flat) layer: i32,
    @location(2) @interpolate(flat) face: u32,
    @location(3) world_pos: vec3<f32>,
    // x = sky-light intensity in [0, 1] (raw byte / 255)
    // y = block-light intensity in [0, 1] (raw byte / 255)
    // The CPU mesher applies inverse-square falloff per cell and
    // averages intensities (not levels) across the 4 blocks around
    // each face corner — the rasterizer just interpolates the
    // averaged bytes, so we get correct soft AO without re-running
    // the curve here.
    @location(4) light: vec2<f32>,
}

@vertex
fn vs_main(in: VsIn) -> VsOut {
    var out: VsOut;
    // World-space position is `in.position` directly because the per-chunk
    // origin is baked into the vertex on upload (see header).
    out.world_pos = in.position;
    out.clip_position = frame.view_proj * vec4<f32>(in.position, 1.0);
    out.uv = in.uv;
    out.layer = i32(in.layer);
    out.face = in.face;
    // Bottom two bytes of `in.light` carry the AO-averaged
    // sky / block intensities (each 0..255). Unpack to [0, 1].
    let sky_intensity = f32(in.light & 0xFFu) / 255.0;
    let block_intensity = f32((in.light >> 8u) & 0xFFu) / 255.0;
    out.light = vec2<f32>(sky_intensity, block_intensity);
    return out;
}

// Pack a unit normal into two channels via octahedral mapping. Returns
// values in `[-1, 1]`; the caller stores them as `[0, 1]` Rg8Unorm via
// `oct_encode_unorm`.
fn oct_encode_signed(n: vec3<f32>) -> vec2<f32> {
    let p = n.xy / (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) {
        let s = vec2<f32>(select(- 1.0, 1.0, p.x >= 0.0), select(- 1.0, 1.0, p.y >= 0.0));
        return (1.0 - abs(p.yx)) * s;
    }
    return p;
}

fn oct_encode_unorm(n: vec3<f32>) -> vec2<f32> {
    return oct_encode_signed(n) * 0.5 + vec2<f32>(0.5);
}

// Warm white for block-emissive sources (torches, glowstone). Slightly
// orange-shifted so block-lit corners read as "firelight" against a
// neutral / cool sky.
const BLOCK_LIGHT_TINT: vec3<f32> = vec3<f32>(1.0, 0.85, 0.65);
// Cool white for sky-derived light. Slightly blue-shifted so daylight
// reads as overcast-sky rather than warm interior.
const SKY_LIGHT_TINT: vec3<f32> = vec3<f32>(0.75, 0.85, 1.0);

// Basic-mode per-face dimming derived from the (interpolated) world
// normal. Selecting on the largest |component| picks the dominant
// face axis: x → 0.5, y → 1.0, z → 0.2. This matches the C++ basic
// rendering values previously baked into the vertex by the mesher.
fn face_dim_factor(n: vec3<f32>) -> f32 {
    let a = abs(n);
    if (a.y >= a.x && a.y >= a.z) {
        return 1.0;
    }
    if (a.x >= a.z) {
        return 0.5;
    }
    return 0.2;
}

// Face-id → world-space normal. Index order matches `chunk_rendering.cpp`:
//   0 = +X (Right), 1 = -X (Left),
//   2 = +Y (Top),   3 = -Y (Bottom),
//   4 = +Z (Front), 5 = -Z (Back).
fn face_normal(face: u32) -> vec3<f32> {
    switch face {
        case 0u : {
            return vec3<f32>(1.0, 0.0, 0.0);
        }
        case 1u : {
            return vec3<f32>(- 1.0, 0.0, 0.0);
        }
        case 2u : {
            return vec3<f32>(0.0, 1.0, 0.0);
        }
        case 3u : {
            return vec3<f32>(0.0, - 1.0, 0.0);
        }
        case 4u : {
            return vec3<f32>(0.0, 0.0, 1.0);
        }
        case 5u : {
            return vec3<f32>(0.0, 0.0, - 1.0);
        }
        default : {
            return vec3<f32>(0.0, 1.0, 0.0);
        }
    }
}

// Tangent-bitangent-normal frame for normal-map sampling. Constructed
// to be right-handed (`cross(T, B) = N`) so the encoded normal in
// tangent space lifts to the correct world-space orientation.
//
// The tangent is hand-picked so the normal map's `r` axis roughly
// aligns with the face's U direction in the C++/Rust `FACE_UVS` table.
// Exact U/V alignment varies face-to-face because our V-flipped UV
// convention (WGSL +Y down) doesn't match the C++ GL convention; for
// most blocks the normal map is roughly symmetric so the slight
// rotation is invisible. The N component (third axis) IS exact.
fn face_tbn(face: u32, n: vec3<f32>) -> mat3x3<f32> {
    var t: vec3<f32>;
    switch face {
        case 0u : {
            t = vec3<f32>(0.0, 0.0, - 1.0);
        }
        // +X face: U ≈ -Z
        case 1u : {
            t = vec3<f32>(0.0, 0.0, 1.0);
        }
        // -X face: U ≈ +Z
        case 2u : {
            t = vec3<f32>(1.0, 0.0, 0.0);
        }
        // +Y face: U ≈ +X
        case 3u : {
            t = vec3<f32>(1.0, 0.0, 0.0);
        }
        // -Y face: U ≈ +X
        case 4u : {
            t = vec3<f32>(1.0, 0.0, 0.0);
        }
        // +Z face: U ≈ +X
        case 5u : {
            t = vec3<f32>(- 1.0, 0.0, 0.0);
        }
        // -Z face: U ≈ -X
        default : {
            t = vec3<f32>(1.0, 0.0, 0.0);
        }
    }
    // `b = cross(N, T)` guarantees `cross(T, B) = N` (right-handed),
    // independent of T's exact direction. The chosen T above is already
    // perpendicular to N for every face id, so no re-orthogonalization
    // is needed.
    let b = cross(n, t);
    return mat3x3<f32>(t, b, n);
}

// Sample the normal-map atlas and lift to world space via the face's
// TBN frame. `(0.5, 0.5, 1.0)` in the atlas decodes to `(0, 0, 1)` in
// tangent space — i.e. "flat" — which leaves the world normal equal to
// the face normal (the third basis vector). Non-flat normals perturb
// in the T / B directions.
fn sample_world_normal(face: u32, uv: vec2<f32>, layer: i32) -> vec3<f32> {
    let face_n = face_normal(face);
    let tbn = face_tbn(face, face_n);
    let texel = textureSample(block_normal, block_sampler, uv, layer).rgb;
    let local = normalize(texel * 2.0 - vec3<f32>(1.0));
    return normalize(tbn * local);
}

// Three MRT outputs into the advanced G-buffer:
//
//   @location(0) diffuse  : Rgba16Float
//                            rgb = raw albedo (composition shades it)
//                            a   = emissive intensity (opaque) /
//                                  texel α (translucent)
//   @location(1) normal   : Rgba8Unorm
//                            rg  = octahedral-encoded world-space
//                                  normal mapped to [0, 1]
//                            b   = per-vertex sky-light intensity
//                                  (0..1) — composition multiplies
//                                  direct sunlight by this so cave /
//                                  overhang occlusion the shadow map
//                                  misses still attenuates lambert
//                            a   = reserved (1.0)
//   @location(2) material : R16Uint
//                                = atlas-layer index (texture id)
struct GBufferOut {
    @location(0) diffuse: vec4<f32>,
    @location(1) normal: vec4<f32>,
    @location(2) material: u32,
}

// Basic-mode fragment shading — single-target output into the
// G-buffer's diffuse attachment (`Rgba16Float`). The composition
// shader's basic entry samples this attachment as the final colour
// for chunk pixels (no sun lambert / shadow / SSR — basic mode
// skips all of that). Two thin entry points (`_opaque` and
// `_translucent`) share this body so the two basic-mode pipelines
// each have their own entry — symmetric with the advanced mode's
// `fs_main_advanced_opaque` / `fs_main_advanced_translucent`.
fn shade_basic(in: VsOut) -> vec4<f32> {
    let sample = textureSample(block_diffuse, block_sampler, in.uv, in.layer);
    if (sample.a <= 0.0) {
        discard;
    }
    // Basic shading model:
    //   sky_visibility  = smoothstep(-0.2, 0.2, sun_dir.y) — daylight
    //                     ramp around the horizon (no separate uniform).
    //   sky_intensity   = sky_level * sky_visibility
    //   block_intensity = block_level
    //   vertex_color    = max(block_intensity * BLOCK_TINT,
    //                         sky_intensity   * SKY_TINT)
    // then apply a per-face directional dimming derived from the
    // world-space normal so side faces read darker than tops.
    let sky_intensity = in.light.x * smoothstep(- 0.2, 0.2, frame.sun_dir.y);
    let block_intensity = in.light.y;
    let block_lit = block_intensity * BLOCK_LIGHT_TINT;
    let sky_lit = sky_intensity * SKY_LIGHT_TINT;
    let lit = max(block_lit, sky_lit);
    let dim = face_dim_factor(face_normal(in.face));
    return vec4<f32>(sample.rgb * lit * dim, sample.a);
}

// Basic-mode opaque + translucent entry points — both delegate to
// `shade_basic`. The two pipelines differ only in blend state (REPLACE
// vs ALPHA_BLENDING), not shading, so the bodies are intentionally
// identical. Two entry points kept for symmetry with advanced mode.
@fragment
fn fs_main_basic_opaque(in: VsOut) -> @location(0) vec4<f32> {
    return shade_basic(in);
}

@fragment
fn fs_main_basic_translucent(in: VsOut) -> @location(0) vec4<f32> {
    // Same opaque-depth discard the advanced translucent uses — the
    // translucent pass owns its own depth attachment so we can't
    // depth-test against opaque via the fixed-function pipeline.
    let pixel = vec2<i32>(in.clip_position.xy);
    let opaque_d = textureLoad(g_opaque_depth, pixel, 0);
    if (in.clip_position.z <= opaque_d) {
        discard;
    }
    return shade_basic(in);
}

// Advanced-mode opaque entry — writes the full opaque-layer G-buffer
// MRT (diffuse + normal + material). Composition runs sun lambert +
// shadow PCF + SSR + emissive against these targets.
@fragment
fn fs_main_advanced_opaque(in: VsOut) -> GBufferOut {
    let sample = textureSample(block_diffuse, block_sampler, in.uv, in.layer);

    // Alpha test — drop fully-transparent texels. Anything else
    // belongs to the opaque layer (translucent water / ice / leaves
    // route to the translucent pipeline / entry point instead).
    if (sample.a <= 0.0) {
        discard;
    }

    let normal = sample_world_normal(in.face, in.uv, in.layer);
    // Advanced rendering: ignore the per-vertex sky light entirely.
    // Composition derives sun + ambient via shadow PCF against the
    // encoded normal; anything we baked here would just double-light.
    // No face dim either — the shadow-map / lambert math handles
    // directional shading correctly.
    let albedo = sample.rgb;
    // Block-light intensity rides into the diffuse alpha as the
    // emissive signal — composition tints it warm and adds it on top
    // so emissive blocks glow even in shadow. (Translucent surfaces
    // never emit, so they use diffuse.a for texel α instead.)
    let emissive = clamp(in.light.y, 0.0, 1.0);

    var out: GBufferOut;
    out.diffuse = vec4<f32>(albedo, emissive);
    out.normal = vec4<f32>(oct_encode_unorm(normal), in.light.x, 1.0);
    out.material = u32(in.layer);
    return out;
}

// Advanced-mode translucent entry — writes the translucent layer's
// G-buffer (diffuse + normal + material). The translucent pass uses
// its own depth attachment so the front-most translucent fragment
// wins; we additionally `discard` here whenever the fragment lies
// behind the opaque depth buffer to avoid stamping translucent
// pixels onto otherwise-occluded geometry.
@fragment
fn fs_main_advanced_translucent(in: VsOut) -> GBufferOut {
    let pixel = vec2<i32>(in.clip_position.xy);
    let opaque_d = textureLoad(g_opaque_depth, pixel, 0);
    // Reversed-Z: bigger value = closer. If we are at-or-behind the
    // front-most opaque fragment, drop.
    if (in.clip_position.z <= opaque_d) {
        discard;
    }

    let sample = textureSample(block_diffuse, block_sampler, in.uv, in.layer);
    if (sample.a <= 0.0) {
        discard;
    }

    let normal = sample_world_normal(in.face, in.uv, in.layer);
    let albedo = sample.rgb;

    var out: GBufferOut;
    // diffuse.a stores the texel alpha verbatim — composition
    // manually mixes the translucent surface over the opaque /
    // sky background. No 0.02 hack; that was a workaround for an
    // alpha-blended G-buffer we no longer use.
    out.diffuse = vec4<f32>(albedo, sample.a);
    out.normal = vec4<f32>(oct_encode_unorm(normal), in.light.x, 1.0);
    out.material = u32(in.layer);
    return out;
}

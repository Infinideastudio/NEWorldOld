// Deferred chunk shader — port of C++ `opaque.{vsh,fsh}` /
// `translucent.{vsh,fsh}` adapted to our `ChunkVertex` layout.
//
// Vertex layout (stride 36 B, matches `gfx::mesh::ChunkVertex`):
//   @location(0) position : vec3<f32>  // offset  0
//   @location(1) uv       : vec2<f32>  // offset 12
//   @location(2) layer    : u32        // offset 20
//   @location(3) face     : u32        // offset 24
//   @location(4) light    : u32        // offset 28 — bottom byte = 0..255
//                                      // smooth-light brightness
//   @location(5) material_id : u32     // offset 32 — 16-bit material id
//
// Per-chunk world origin: baked into `position` at upload time on the CPU
// (`ChunkMesh::upload` adds `coord * CHUNK_SIZE` to every vertex's position).
// That keeps the shader free of per-chunk uniforms; only the camera's
// view-projection is consumed here.
//
// Lighting model: matches the C++ basic rendering mode (`default.fsh`).
// Vertex pass exposes the per-vertex AO-averaged smooth-light brightness;
// fragment pass writes `albedo * brightness` straight into the G-buffer
// diffuse target. No lambert, no fog, no ambient sky — those belong to
// the advanced-mode `final.fsh` which the migration plan deliberately
// leaves unported.
//
// The fragment writes three G-buffer targets — diffuse / normal / material —
// the composition pass (`shaders/composition.wgsl`) consumes. Composition
// today is just a blit (basic-mode equivalent); future advanced-mode work
// (shadow PCF, SSR, volumetric clouds) plugs into the same G-buffer.
//
// Bind groups:
//   group 0 binding 0 : FrameUniforms (uniform buffer)
//   group 1 binding 0 : block_diffuse texture_2d_array<f32>
//   group 1 binding 1 : block_sampler sampler

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
    shadow_params: vec4<f32>,
    player_coord_int: vec4<i32>,
    player_coord_mod: vec4<i32>,
    player_coord_frac: vec4<f32>,
}

;

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

struct VsIn {
    @location(0) position: vec3<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) layer: u32,
    @location(3) face: u32,
    @location(4) light: u32,
    @location(5) material_id: u32,
}

;

struct VsOut {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) @interpolate(flat) layer: i32,
    @location(2) @interpolate(flat) face: u32,
    @location(3) world_pos: vec3<f32>,
    @location(4) light: f32,
    @location(5) @interpolate(flat) material_id: u32,
}

;

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
    // Bottom byte of `in.light` is 0..255 brightness; the rasterizer
    // interpolates this linearly across the four corners of each face,
    // giving smooth lighting / soft AO falloff.
    out.light = f32(in.light & 0xFFu) / 255.0;
    out.material_id = in.material_id;
    return out;
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

// Three MRT outputs into the deferred G-buffer (matching C++ formats):
//
//   @location(0) diffuse  : rgba32_float
//                            rgb = pre-modulated albedo * smooth-light brightness
//                            a   = translucency hint (1.0 opaque, < 1.0 see-through)
//   @location(1) normal   : rgba8_unorm
//                            rgb = (n + 1) * 0.5  (encoded world-space normal)
//   @location(2) material : rgba8_unorm
//                            rg  = encode_u16(material_id) — high byte / low byte
//                            ba  = (0, 1) — composition decodes via hi*256 + lo
//                            (material_id == 0 reserved for "no fragment / sky")
struct GBufferOut {
    @location(0) diffuse: vec4<f32>,
    @location(1) normal: vec4<f32>,
    @location(2) material: vec4<f32>,
}

;

// Mirrors the C++ `encode_u16` helper from `opaque.fsh` — splits a u16
// across the R and G channels of an Rgba8Unorm texel. Composition pairs
// this with `decode_u16(rg) = hi*256 + lo` to recover the block id.
fn encode_u16(v: u32) -> vec2<f32> {
    let hi = (v >> 8u) & 0xFFu;
    let lo = v & 0xFFu;
    return vec2<f32>(f32(hi) / 255.0, f32(lo) / 255.0);
}

// Forward fragment for translucent chunks — basic-mode equivalent.
//
// Translucent water / leaves can't go through the deferred path because
// `Rgba32Float` G-buffer targets aren't blendable in wgpu without the
// `Float32Blendable` feature. So translucent chunks render forward to
// the surface (Bgra8UnormSrgb) AFTER composition, with standard
// `SrcAlpha / OneMinusSrcAlpha` blending — matches the C++ basic-mode
// translucent pass which writes through `default.fsh` to the swap chain
// with `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` enabled.
//
// Reads the same vertex/uniform/sampler bindings as `fs_main`.
@fragment
fn fs_main_forward(in: VsOut) -> @location(0) vec4<f32> {
    let sample = textureSample(block_diffuse, block_sampler, in.uv, in.layer);
    if (sample.a <= 0.0) {
        discard;
    }
    // C++ default.fsh: `o_frag_color = vec4(color, 1.0) * texel`. Our
    // smooth-light brightness already carries the per-face dimming
    // baked in by `mesh::apply_face_dim`, so the lit color is just
    // `texel.rgb * brightness` and the alpha pass-through preserves
    // water / glass / ice translucency.
    let brightness = in.light;
    return vec4<f32>(sample.rgb * brightness, sample.a);
}

@fragment
fn fs_main(in: VsOut) -> GBufferOut {
    let sample = textureSample(block_diffuse, block_sampler, in.uv, in.layer);

    // Alpha test — discard fully-transparent texels only. Mirrors the C++
    // `opaque.fsh` (`if (texel.a <= 0.0) discard;`); a stricter `< 0.5`
    // throws away translucent surfaces like water that we still want to
    // blend through the translucent pipeline.
    if (sample.a <= 0.0) {
        discard;
    }

    let normal = sample_world_normal(in.face, in.uv, in.layer);
    // Smooth-light brightness — same value the C++ build packs into the
    // chunk vertex's `a_color` attribute. `default.fsh` then does
    // `vec4(color, 1.0) * texel`, giving the texel color scaled by the
    // per-vertex AO-averaged sky+block-light brightness. We preserve
    // that exact behavior — no 0.35 floor, no lambert, no fog. Cells
    // with zero light go fully dark, matching basic mode.
    let brightness = in.light;
    let albedo_lit = sample.rgb * brightness;

    var out: GBufferOut;
    // The albedo is already lit by smooth lighting (vertex-interpolated),
    // which the composition pass keeps so per-vertex AO survives unchanged
    // through the deferred path. Sun directional lighting is applied in the
    // composition pass against the encoded world-space normal.
    out.diffuse = vec4<f32>(albedo_lit, sample.a);
    out.normal = vec4<f32>(normal * 0.5 + vec3<f32>(0.5), 1.0);
    // Block id (16-bit) encoded as two bytes (R = high, G = low) — same
    // as C++ `opaque.fsh` / `translucent.fsh` so the composition shader
    // is a direct port. `material_id == 0` reads as `(0, 0)` and signals
    // "no fragment / sky" to composition (cleared material texels also
    // read as zero — air never produces a fragment, so the convention
    // is unambiguous).
    out.material = vec4<f32>(encode_u16(in.material_id), 0.0, 1.0);
    return out;
}

// Translucent G-buffer entry point — used by `translucent_gbuffer_pipeline`.
// Differs from `fs_main` only in forcing alpha to `0.02` for water / ice
// blocks, mirroring C++ `translucent.fsh`. Composition's SSR + Schlick
// fresnel relies on this near-transparent base so the reflected sky /
// SSR sample dominates the visible water surface colour. Alpha-blended
// against the opaque diffuse already in the G-buffer; depth-writes the
// water surface so composition's SSR raymarch can hit opaque geometry
// behind the water.
const SSR_WATER_ID: u32 = 21u;
const SSR_ICE_ID: u32 = 26u;

@fragment
fn fs_main_translucent(in: VsOut) -> GBufferOut {
    let sample = textureSample(block_diffuse, block_sampler, in.uv, in.layer);
    if (sample.a <= 0.0) {
        discard;
    }

    let normal = sample_world_normal(in.face, in.uv, in.layer);
    let brightness = in.light;
    let albedo_lit = sample.rgb * brightness;

    var alpha = sample.a;
    if (in.material_id == SSR_WATER_ID || in.material_id == SSR_ICE_ID) {
        alpha = 0.02;
    }

    var out: GBufferOut;
    out.diffuse = vec4<f32>(albedo_lit, alpha);
    out.normal = vec4<f32>(normal * 0.5 + vec3<f32>(0.5), 1.0);
    out.material = vec4<f32>(encode_u16(in.material_id), 0.0, 1.0);
    return out;
}

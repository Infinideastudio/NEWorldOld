// Forward / "basic rendering" chunk shader — port of `default.{vsh,fsh}`.
//
// This is the C++ basic-mode chunk shader: the rasterizer just samples the
// diffuse atlas and multiplies by the per-vertex smooth-light brightness.
// No lambert, no fog, no ambient sky bounce — those are advanced-mode
// features that live in `final.fsh` (deliberately not ported per the
// migration plan's Tier 4 scope).
//
// **Adapted vertex layout.** The C++ build uses
// `Color<Vec3u8>` + `TexCoord<Vec3f>` (where the third tex-coord component
// is the atlas layer). Our Rust port packs the equivalent data into the
// `ChunkVertex` defined in `src/render/mesh.rs`:
//   * `light: u32`  — bottom byte = AO-averaged brightness 0..255.
//   * `layer: u32`  — atlas array layer (was tex_coord.z in C++).
//   * `uv: vec2<f32>` — atlas s/t (was tex_coord.xy in C++).
// This shader uses our layout — i.e., the WGSL port of the C++ shader's
// *meaning*, not its literal attribute list. Wiring the literal C++
// layout would require a vertex-format refactor that's out of scope.
//
// The deferred chunk shader (`chunk.wgsl`) writes into a G-buffer; this
// forward shader writes the lit color directly to its single color
// attachment. Today neither pipeline nor surface uses this shader — the
// deferred path covers the same ground — but the WGSL is here for parity
// and as a reference for any future basic-mode pipeline switch.
//
// Bind groups (mirrors the deferred path so it slots into the same pipeline layout):
//   group 0 binding 0 : FrameUniforms
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

@group(0) @binding(0)
var<uniform> frame: FrameUniforms;
@group(1) @binding(0)
var block_diffuse: texture_2d_array<f32>;
@group(1) @binding(1)
var block_sampler: sampler;

struct VsIn {
    @location(0) position: vec3<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) layer: u32,
    @location(3) face: u32,
    @location(4) light: u32,
    @location(5) material_id: u32,
}

struct VsOut {
    @builtin(position) clip_position: vec4<f32>,
    // C++ uses `centroid in vec3 ...` for this varying. WGSL spells it
    // `@interpolate(perspective, centroid)` — same multisample-aware
    // interpolation, just different syntax.
    @location(0) @interpolate(perspective, centroid) uv: vec2<f32>,
    @location(1) @interpolate(flat) layer: i32,
    @location(2) @interpolate(perspective, centroid) brightness: f32,
}

@vertex
fn vs_main(in: VsIn) -> VsOut {
    var out: VsOut;
    out.clip_position = frame.view_proj * vec4<f32>(in.position, 1.0);
    out.uv = in.uv;
    out.layer = i32(in.layer);
    // Bottom byte of `light` is the smooth-light brightness — same value
    // C++ stores in all three channels of `Color: Vec3u8`.
    out.brightness = f32(in.light & 0xFFu) / 255.0;
    return out;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4<f32> {
    let texel = textureSample(block_diffuse, block_sampler, in.uv, in.layer);
    // Mirrors the C++ `if (texel.a <= 0.0) discard;` test.
    if (texel.a <= 0.0) {
        discard;
    }
    // C++ does `vec4(color, 1.0) * texel`; with monochrome brightness
    // (same value in r/g/b) that's `vec4(brightness * texel.rgb, texel.a)`.
    return vec4<f32>(texel.rgb * in.brightness, texel.a);
}

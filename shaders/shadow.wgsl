// Sun-POV shadow-map writer — port of `shadow.{vsh,fsh}`.
//
// Renders the chunk geometry from the sun's point of view into the
// shadow depth attachment. The fragment is mostly a no-op (just an
// alpha test on the diffuse atlas); only the depth write matters.
//
// **Adapted vertex layout.** Same `ChunkVertex` layout as `chunk.wgsl`
// — the C++ build uses `Color: Vec3u8` / `TexCoord: Vec3f` while we
// pack equivalents into `light` / `(uv, layer)`. The shadow pass only
// reads `position`, `uv`, `layer`, `material_id`; `light` and `face` are
// ignored (no lighting in this pass).
//
// Fisheye projection: the C++ build warps the post-perspective xy by
//   p' = p / ((1 - k) + |p| * k)
// where `k = u_shadow_fisheye_factor`. The warp is applied AFTER the
// perspective divide so it operates in NDC space — same as C++. wgpu's
// rasterizer will re-project from clip space, but since `clip.w = 1`
// after the explicit divide the warp survives.
//
// Bind groups (shared with the deferred chunk shader so the same
// pipeline layout slots in):
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

const PI: f32 = 3.1415926;

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
    @location(0) @interpolate(perspective, centroid) uv: vec2<f32>,
    @location(1) @interpolate(flat) layer: i32,
}

// `shadow_params.z` holds the fisheye factor (see C++ `u_shadow_fisheye_factor`).
//
// The fisheye anchor is the player's projected clip-xy — i.e. the centre
// of the shadow viewport. C++ gets this for free because its vertices are
// camera-relative (`u_translation = chunk - camera` is pushed per-draw),
// so `shadow_mvp * (0,0,0,1)` IS the player's clip-space position. The
// Rust port runs against world-space vertices, so projecting world-origin
// would land far outside `[-1, 1]` whenever the player isn't near the
// world origin — and the warp would push every nearby chunk vertex
// off-screen. Project `camera_pos` instead to recover C++ parity.
fn fisheye_origin() -> vec2<f32> {
    let p = frame.shadow_view_proj * vec4<f32>(frame.camera_pos.xyz, 1.0);
    return p.xy / p.w;
}

fn fisheye_project(p: vec2<f32>) -> vec2<f32> {
    let origin = fisheye_origin();
    let local = p - origin;
    let dist = length(local);
    let k = frame.shadow_params.z;
    let distort = (1.0 - k) + dist * k;
    return local / distort + origin;
}

@vertex
fn vs_main(in: VsIn) -> VsOut {
    var out: VsOut;
    var coord = in.position;
    var pos = frame.shadow_view_proj * vec4<f32>(coord, 1.0);
    // Apply the perspective divide explicitly so the fisheye warp lands
    // in NDC space — matches C++ `gl_Position /= gl_Position.w` then
    // `gl_Position = vec4(fisheye_projection(gl_Position.xy), gl_Position.zw)`.
    pos /= pos.w;
    let warped_xy = fisheye_project(pos.xy);
    out.clip_position = vec4<f32>(warped_xy, pos.z, 1.0);
    out.uv = in.uv;
    out.layer = i32(in.layer);
    return out;
}

@fragment
fn fs_main(in: VsOut) {
    let texel = textureSample(block_diffuse, block_sampler, in.uv, in.layer);
    // Same alpha test as the regular chunk pass — discarded leaves /
    // non-rectangular blocks must NOT contribute to the shadow map, so
    // the discard fires before depth write.
    if (texel.a <= 0.0) {
        discard;
    }
    // C++ `shadow.fsh` writes `o_frag_color = vec4(1.0)` to a debug
    // color attachment. wgpu allows depth-only render passes, so the
    // shadow pipeline drops the color attachment entirely and this
    // fragment shader just controls discard / depth write.
}

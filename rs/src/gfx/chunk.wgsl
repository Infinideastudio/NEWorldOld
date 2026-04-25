// Forward chunk shader for [D2] + [D4] (final pass / fog).
//
// Vertex layout (stride 28 B, matches `gfx::mesh::ChunkVertex`):
//   @location(0) position : vec3<f32>  // offset  0
//   @location(1) uv       : vec2<f32>  // offset 12
//   @location(2) layer    : u32        // offset 20
//   @location(3) face     : u32        // offset 24
//
// Per-chunk world origin: baked into `position` at upload time on the CPU
// (`ChunkMesh::upload` adds `coord * CHUNK_SIZE` to every vertex's position).
// That keeps the shader free of per-chunk uniforms; only the camera's
// view-projection is consumed here.
//
// [D4] folds a "final pass"-style distance fog into this same shader rather
// than adding a separate deferred composition pass — the migration plan's
// `final.fsh` worth of compositing is far more than minimum-viable needs.
// Far chunks blend toward `SKY_COLOR`, which is also what the App clears the
// surface to, so missing chunks fade seamlessly into the horizon.
//
// Bind groups:
//   group 0 binding 0 : FrameUniforms (uniform buffer)
//   group 1 binding 0 : block_diffuse texture_2d_array<f32>
//   group 1 binding 1 : block_sampler sampler

struct FrameUniforms {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    view_proj: mat4x4<f32>,
    camera_pos: vec4<f32>,
    sun_dir: vec4<f32>,
    screen_size: vec2<f32>,
    time: f32,
    _pad: f32,
};

@group(0) @binding(0) var<uniform> frame: FrameUniforms;
@group(1) @binding(0) var block_diffuse: texture_2d_array<f32>;
@group(1) @binding(1) var block_sampler: sampler;

// [D4] Horizon-blue sky color. Matches the App's surface clear color so the
// fog falloff lands on the same value the user already sees behind the world.
const SKY_COLOR: vec3<f32> = vec3<f32>(0.55, 0.72, 0.92);

// [D4] Fog distance band. Far blocks beyond `FOG_END` are pure sky color.
const FOG_START: f32 = 24.0;
const FOG_END: f32 = 96.0;

// Fraction of `SKY_COLOR` mixed back into the lit color as ambient sky-bounce.
// Keeps shadowed faces from going pitch-black on a sunlit scene.
const AMBIENT_SKY: f32 = 0.25;

struct VsIn {
    @location(0) position: vec3<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) layer: u32,
    @location(3) face: u32,
};

struct VsOut {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) @interpolate(flat) layer: i32,
    @location(2) @interpolate(flat) face: u32,
    @location(3) world_pos: vec3<f32>,
};

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
    return out;
}

// Face-id → world-space normal. Index order matches `chunk_rendering.cpp`:
//   0 = +X (Right), 1 = -X (Left),
//   2 = +Y (Top),   3 = -Y (Bottom),
//   4 = +Z (Front), 5 = -Z (Back).
fn face_normal(face: u32) -> vec3<f32> {
    switch face {
        case 0u: { return vec3<f32>( 1.0,  0.0,  0.0); }
        case 1u: { return vec3<f32>(-1.0,  0.0,  0.0); }
        case 2u: { return vec3<f32>( 0.0,  1.0,  0.0); }
        case 3u: { return vec3<f32>( 0.0, -1.0,  0.0); }
        case 4u: { return vec3<f32>( 0.0,  0.0,  1.0); }
        case 5u: { return vec3<f32>( 0.0,  0.0, -1.0); }
        default: { return vec3<f32>( 0.0,  1.0,  0.0); }
    }
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4<f32> {
    let sample = textureSample(block_diffuse, block_sampler, in.uv, in.layer);

    // Alpha test for foliage / glass cutouts. Translucent surfaces (water)
    // pass through and are blended by the translucent pipeline state.
    if (sample.a < 0.5) {
        discard;
    }

    let normal = face_normal(in.face);
    let ndotl = max(dot(normal, frame.sun_dir.xyz), 0.0);
    let lambert = mix(0.3, 1.0, ndotl);
    var rgb = sample.rgb * lambert;

    // [D4] Ambient sky-bounce: pull shadowed surfaces a little toward sky
    // tint so the dark side of geometry isn't a flat charcoal.
    rgb = mix(rgb, rgb * SKY_COLOR + SKY_COLOR * 0.05, AMBIENT_SKY * (1.0 - ndotl));

    // [D4] Distance fog: linear band between FOG_START and FOG_END, blended
    // toward `SKY_COLOR` so far blocks dissolve into the horizon.
    let dist = length(in.world_pos - frame.camera_pos.xyz);
    let fog = clamp((dist - FOG_START) / (FOG_END - FOG_START), 0.0, 1.0);
    rgb = mix(rgb, SKY_COLOR, fog);

    return vec4<f32>(rgb, sample.a);
}

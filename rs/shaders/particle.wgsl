// Particle billboard shader.
//
// Vertex stage: each per-particle quad has 6 vertices that all share the
// particle's `world_pos` + `size`; the corner offset is reconstructed from
// the per-vertex `uv` (`uv == (0,0)` is the bottom-left of the quad,
// `(1,1)` is top-right). The corner is added to `world_pos` along the
// camera right/up basis vectors, derived from the view matrix.
//
// `frame.view` is the world→view matrix. Its rows are the camera-space
// basis vectors expressed in world space, so:
//   right_ws = vec3(view[0][0], view[1][0], view[2][0])
//   up_ws    = vec3(view[0][1], view[1][1], view[2][1])
//
// Fragment stage: sample `block_diffuse`, alpha-test at 0.5.

struct FrameUniforms {
    view:        mat4x4<f32>,
    proj:        mat4x4<f32>,
    view_proj:   mat4x4<f32>,
    camera_pos:  vec4<f32>,
    sun_dir:     vec4<f32>,
    screen_size: vec2<f32>,
    time:        f32,
    _pad:        f32,
};

@group(0) @binding(0) var<uniform> frame: FrameUniforms;

@group(1) @binding(0) var block_diffuse: texture_2d_array<f32>;
@group(1) @binding(1) var block_sampler: sampler;

struct VsIn {
    @location(0) world_pos: vec3<f32>,
    @location(1) size:      f32,
    @location(2) uv:        vec2<f32>,
    @location(3) layer:     u32,
};

struct VsOut {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv:    vec2<f32>,
    @location(1) layer: u32,
};

@vertex
fn vs_main(in: VsIn) -> VsOut {
    // Camera-space right / up in world space — read from the view matrix
    // columns. WGSL `mat4x4<f32>` is column-major and accessed as
    // `m[col][row]`; what we want is row 0 / row 1 of the view matrix
    // (which are the world-space directions of camera +X / +Y).
    let right_ws = vec3<f32>(frame.view[0][0], frame.view[1][0], frame.view[2][0]);
    let up_ws    = vec3<f32>(frame.view[0][1], frame.view[1][1], frame.view[2][1]);

    // Map uv from [0,1] to [-1,+1] so the corner offset is centered on
    // `world_pos`.
    let off = (in.uv * 2.0 - vec2<f32>(1.0, 1.0)) * in.size;

    let world_pos = in.world_pos + right_ws * off.x + up_ws * off.y;
    let clip = frame.view_proj * vec4<f32>(world_pos, 1.0);

    var out: VsOut;
    out.clip_position = clip;
    out.uv = in.uv;
    out.layer = in.layer;
    return out;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4<f32> {
    let color = textureSample(block_diffuse, block_sampler, in.uv, i32(in.layer));
    if (color.a < 0.5) {
        discard;
    }
    return color;
}

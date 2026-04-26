// Shadow-map visualizer — port of `debug_shadow.{vsh,fsh}`.
//
// Renders the shadow map to a screen-space quad so the user can verify
// that the sun-POV depth pass is doing the right thing. Mirrors the
// C++ debug shader, including its binary-search trick to recover the
// stored depth value via repeated comparison samples.
//
// **Adapted vertex layout.** The C++ `debug_shadow.vsh` consumes
//   layout(location = 0) in vec2 a_coord;
//   layout(location = 1) in vec2 a_tex_coord;
// driven by a host-built quad VBO. WGSL synthesizes the same quad from
// `@builtin(vertex_index)` instead — no vertex buffer required.
//
// Why the binary search? The shadow texture is a depth texture sampled
// through a comparison sampler (`sampler_comparison` / `sampler2DArrayShadow`
// in C++). `textureSampleCompare` returns 1.0 if the test reference
// passes, 0.0 if not. To recover the stored depth, the C++ build does
// 8 iterations of binary search on the reference value, narrowing in on
// the actual depth in `[0, 1]`. We do the same.
//
// Bind groups (uses the existing composition aux group so we can share
// the same shadow texture / sampler bindings):
//   group 0 binding 0 : shadow_texture : texture_depth_2d
//   group 0 binding 1 : shadow_sampler : sampler_comparison

@group(0) @binding(0) var shadow_texture: texture_depth_2d;
@group(0) @binding(1) var shadow_sampler: sampler_comparison;

struct VsOut {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

// Six-vertex full-screen quad — same trick as the composition shader.
// The C++ debug pass has the host program upload a quad VBO with custom
// dimensions; we just present full-screen by default. A future pipeline
// can shrink the quad by supplying a `Filter`-style transform uniform.
@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
    let positions = array<vec2<f32>, 6>(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>( 1.0, -1.0),
        vec2<f32>( 1.0,  1.0),
        vec2<f32>(-1.0, -1.0),
        vec2<f32>( 1.0,  1.0),
        vec2<f32>(-1.0,  1.0),
    );
    let p = positions[vid];
    var out: VsOut;
    out.clip_position = vec4<f32>(p, 0.0, 1.0);
    out.uv = vec2<f32>(p.x * 0.5 + 0.5, 1.0 - (p.y * 0.5 + 0.5));
    return out;
}

// Mirrors C++ `sample_shadow`: 8-step binary search of the reference
// value, returning the recovered depth in `[0, 1]`.
fn recover_shadow_depth(uv: vec2<f32>) -> f32 {
    var first: f32 = 0.0;
    var last: f32 = 1.0;
    var mid: f32 = 0.5;
    for (var i: i32 = 0; i < 8; i++) {
        mid = (first + last) * 0.5;
        let pass_ratio = textureSampleCompare(shadow_texture, shadow_sampler, uv, mid);
        // C++ uses `< 0.5` as "passed" — same here. With our reversed-Z
        // `GreaterEqual` compare, the stored depth is at the bottom of
        // the search; basic-mode parity intent: we just port the C++
        // logic verbatim. (When the shadow map starts emitting real
        // data the comparison direction may need a sign flip — see the
        // mismatch report at the bottom of this changeset.)
        if (pass_ratio < 0.5) {
            first = mid;
        } else {
            last = mid;
        }
    }
    return mid;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4<f32> {
    let texel = recover_shadow_depth(in.uv);
    // C++ behavior: depth ≈ 0 (shadow map cleared / outside the sun's
    // render volume) → semi-transparent grey. Otherwise, grayscale of
    // the recovered depth value.
    if (texel < 1.0 / 255.0) {
        return vec4<f32>(0.2, 0.2, 0.2, 0.5);
    }
    return vec4<f32>(texel, texel, texel, 1.0);
}

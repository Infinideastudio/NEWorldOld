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
// Bind groups:
//   group 0 binding 0 : shadow_texture : texture_depth_2d
//   group 0 binding 1 : shadow_sampler : sampler_comparison
//   group 1 binding 0 : DebugShadowUniforms — NDC quad bounds the
//                       host-side overlay pipeline picks (top-right
//                       square, square sized by aspect ratio, etc).

@group(0) @binding(0) var shadow_texture: texture_depth_2d;
@group(0) @binding(1) var shadow_sampler: sampler_comparison;

struct DebugShadowUniforms {
    // (xi, yi, xa, ya) in NDC: top-left = (xi, yi), bottom-right = (xa, ya).
    // C++ convention from `neworld.ixx::1034`: yi = 1.0 (top), ya = 0.0
    // (middle of screen), xi = 1 - h/w, xa = 1.0 — a square in the top-right.
    quad: vec4<f32>,
};

@group(1) @binding(0) var<uniform> debug_shadow: DebugShadowUniforms;

struct VsOut {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

// Two-triangle quad placed in NDC at `debug_shadow.quad`. Texture-space
// V is flipped vs the C++ port so the shadow map sampled here looks the
// same as the C++ debug overlay (GL `t = 0` is at the bottom of the
// texture; WGSL `t = 0` is at the top).
@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
    let xi = debug_shadow.quad.x;
    let yi = debug_shadow.quad.y;
    let xa = debug_shadow.quad.z;
    let ya = debug_shadow.quad.w;
    // CCW winding: bottom-left, bottom-right, top-right then bottom-left,
    // top-right, top-left. front_face: Ccw on the host pipeline rejects
    // the alternative.
    let positions = array<vec2<f32>, 6>(
        vec2<f32>(xi, ya),
        vec2<f32>(xa, ya),
        vec2<f32>(xa, yi),
        vec2<f32>(xi, ya),
        vec2<f32>(xa, yi),
        vec2<f32>(xi, yi),
    );
    let uvs = array<vec2<f32>, 6>(
        vec2<f32>(0.0, 1.0), // bottom-left
        vec2<f32>(1.0, 1.0), // bottom-right
        vec2<f32>(1.0, 0.0), // top-right
        vec2<f32>(0.0, 1.0),
        vec2<f32>(1.0, 0.0),
        vec2<f32>(0.0, 0.0), // top-left
    );
    var out: VsOut;
    out.clip_position = vec4<f32>(positions[vid], 0.0, 1.0);
    out.uv = uvs[vid];
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

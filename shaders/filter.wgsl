// Generic post-process filter — port of `filter.{vsh,fsh}`.
//
// Single-input, single-output full-screen pass. Currently implements
// a separable Gaussian blur (horizontal pass = filter id 1, vertical
// pass = filter id 2); other filter ids fall through to a black clear.
// Mirrors the C++ build's `filter.fsh` exactly, including the runtime
// `for` loop with float bounds.
//
// **Adapted vertex layout.** The C++ `filter.vsh` consumes a host-built
// quad VBO with `vec2 a_coord` (in pixels) + `vec2 a_tex_coord`. WGSL
// synthesizes a full-screen quad from `@builtin(vertex_index)` instead
// — the sub-rect filtering the C++ shader supports (via `a_coord` in
// pixel space) is not used by any caller in the migration plan.
//
// **Texture binding shape.** C++ declares `sampler2DArray u_buffer` and
// samples it with `texture(u_buffer, vec3(uv, 0.0))`. The "array" view
// is technically wrong in C++ (the bound texture is 2D; OpenGL drivers
// accept it because most permit sampling a 2D texture as a single-layer
// 2D array). WGSL is strict — we declare `texture_2d<f32>` here and
// drop the `0.0` z component.
//
// Bind groups:
//   group 0 binding 0 : FilterUniforms (uniform buffer)
//   group 1 binding 0 : input_texture  : texture_2d<f32>
//   group 1 binding 1 : input_sampler  : sampler

const PI: f32 = 3.141593;

struct FilterUniforms {
    buffer_width: f32,
    buffer_height: f32,
    filter_id: i32,
    gaussian_blur_radius: f32,
    gaussian_blur_step_size: f32,
    gaussian_blur_sigma: f32,
    _pad0: f32,
    _pad1: f32,
};

@group(0) @binding(0) var<uniform> filter_uniforms: FilterUniforms;
@group(1) @binding(0) var input_texture: texture_2d<f32>;
@group(1) @binding(1) var input_sampler: sampler;

struct VsOut {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

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
    // Same Y-flip as composition.wgsl — wgpu NDC has +Y up, our
    // texture-space convention has +Y down.
    out.uv = vec2<f32>(p.x * 0.5 + 0.5, 1.0 - (p.y * 0.5 + 0.5));
    return out;
}

fn sample_color(uv: vec2<f32>) -> vec4<f32> {
    return textureSampleLevel(input_texture, input_sampler, uv, 0.0);
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4<f32> {
    // C++ filter ids: 1 = horizontal Gaussian, 2 = vertical Gaussian.
    // Anything else clears to black — same fallback the C++ shader has.
    if (filter_uniforms.filter_id == 1 || filter_uniforms.filter_id == 2) {
        let radius = filter_uniforms.gaussian_blur_radius;
        let step_size = max(filter_uniforms.gaussian_blur_step_size, 1e-3);
        let sigma2 = filter_uniforms.gaussian_blur_sigma * filter_uniforms.gaussian_blur_sigma;
        let horizontal = filter_uniforms.filter_id == 1;
        var sum = vec4<f32>(0.0);
        var total: f32 = 0.0;
        // Runtime-bounded loop. WGSL accepts non-constant bounds —
        // performance scales with `radius`, same as the GLSL original.
        var x: f32 = -radius;
        loop {
            if (x > radius) { break; }
            let weight = (1.0 / sqrt(2.0 * PI * sigma2)) * exp(-(x * x) / (2.0 * sigma2));
            var sample_uv = in.uv;
            if (horizontal) {
                sample_uv.x += x / filter_uniforms.buffer_width;
            } else {
                sample_uv.y += x / filter_uniforms.buffer_height;
            }
            sum += weight * sample_color(sample_uv);
            total += weight;
            x += step_size;
        }
        return sum / max(total, 1e-6);
    }
    return vec4<f32>(0.0);
}

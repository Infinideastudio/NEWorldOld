// Deferred composition pass — basic-mode equivalent.
//
// Reads the G-buffer at every pixel and writes the lit color to the
// surface. Today the lighting model matches the C++ `default.fsh`
// (basic rendering mode): per-vertex smooth-light is already baked into
// `gbuffer.diffuse.rgb` by `chunk.wgsl`, so composition is essentially a
// blit. The shadow / SSR / volumetric-cloud bindings are wired but stay
// unused — they belong to the upcoming advanced-mode `final.fsh` port.
//
// Sky background: pixels with `material == 0` (no chunk fragment was
// drawn) are filled with a flat sky color, mirroring the C++ basic mode
// where the framebuffer's clear color shows through wherever no chunk
// painted. The forward overlay pass that follows then composites
// particles / selection / underwater on top of this surface.
//
// Bind groups:
//   group 0 binding 0 : FrameUniforms                   (uniform buffer)
//   group 1 binding 0 : g_diffuse  : texture_2d<f32>    (Rgba32Float)
//   group 1 binding 1 : g_normal   : texture_2d<f32>    (Rgba8Unorm)
//   group 1 binding 2 : g_material : texture_2d<f32>    (Rgba8Unorm; R/G hold u16)
//   group 1 binding 3 : g_depth    : texture_depth_2d   (Depth32Float)
//   group 2 binding 0 : shadow_texture : texture_depth_2d
//   group 2 binding 1 : shadow_sampler : sampler_comparison
//   group 2 binding 2 : noise_texture  : texture_2d<f32>
//   group 2 binding 3 : noise_sampler  : sampler
//
// The shadow + noise bindings are wired now so subsequent advanced-mode
// work (shadow pass, SSR, volumetric clouds) can consume them without
// further pipeline changes.

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
};

@group(0) @binding(0) var<uniform> frame: FrameUniforms;

@group(1) @binding(0) var g_diffuse: texture_2d<f32>;
@group(1) @binding(1) var g_normal: texture_2d<f32>;
@group(1) @binding(2) var g_material: texture_2d<f32>;
@group(1) @binding(3) var g_depth: texture_depth_2d;

@group(2) @binding(0) var shadow_texture: texture_depth_2d;
@group(2) @binding(1) var shadow_sampler: sampler_comparison;
@group(2) @binding(2) var noise_texture: texture_2d<f32>;
@group(2) @binding(3) var noise_sampler: sampler;

// Sky tint. Matches the App's surface clear color so far-distance
// missing chunks fade seamlessly into the cleared backdrop. Mirrors
// what the C++ basic-mode build sees — the framebuffer clear color
// takes over wherever no chunk was painted.
const SKY_COLOR: vec3<f32> = vec3<f32>(0.55, 0.72, 0.92);

struct VsOut {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

// Full-screen triangle / quad — six vertices over two triangles.
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
    // Pixel-space UV: (0, 0) at top-left, (1, 1) at bottom-right.
    // WGSL/wgpu's NDC has +Y up; our texture-space convention has +Y
    // down (row 0 is the top), so we flip Y here.
    out.uv = vec2<f32>(p.x * 0.5 + 0.5, 1.0 - (p.y * 0.5 + 0.5));
    return out;
}

// Mirrors the C++ `decode_u16` helper in `final.fsh`: the chunk shader
// stored `(hi/255, lo/255, 0, 1)` so we recover `hi*256 + lo`.
fn decode_u16(v: vec2<f32>) -> u32 {
    let hi = u32(v.x * 255.0 + 0.5);
    let lo = u32(v.y * 255.0 + 0.5);
    return hi * 256u + lo;
}

// Touch the shadow + noise bindings so wgpu's pipeline reflection
// keeps them live. Today the contributions are zero-scale; the
// upcoming `final.fsh` port lights them up for real.
fn aux_zero_contribution(uv: vec2<f32>) -> f32 {
    let s = textureSampleCompare(shadow_texture, shadow_sampler, vec2<f32>(0.5, 0.5), 0.0);
    let n = textureSampleLevel(noise_texture, noise_sampler, uv, 0.0).r;
    // Multiplier kept at 0 so neither sample contributes to the output;
    // the calls exist purely to anchor the bindings in the shader's
    // reflection so wgpu doesn't complain about an unused binding when
    // we later swap in the real shadow / noise math.
    return (s + n) * 0.0;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4<f32> {
    let pixel = vec2<i32>(in.clip_position.xy);
    let material = decode_u16(textureLoad(g_material, pixel, 0).rg);

    // Material 0 means "no chunk fragment was drawn at this pixel" —
    // composition fills with the flat sky color, matching basic-mode's
    // framebuffer-clear behavior.
    if (material == 0u) {
        return vec4<f32>(SKY_COLOR, 1.0);
    }

    let diffuse = textureLoad(g_diffuse, pixel, 0);
    // C++ `default.fsh` is `o_frag_color = vec4(color, 1.0) * texel`,
    // and the chunk shader has already pre-multiplied texel × brightness
    // into `diffuse.rgb`. So we just present.
    var rgb = diffuse.rgb;

    // Anchor the shadow / noise bindings; zero-scale, no visual impact.
    rgb += vec3<f32>(aux_zero_contribution(in.uv));

    return vec4<f32>(rgb, diffuse.a);
}

// Advanced-mode deferred composition pass — port of `final.fsh`.
//
// Reads the G-buffer, applies sun lambert + ambient with optional SSAO
// and shadow PCF, blends through distance fog into a directional sky
// tint, optionally raymarches volumetric clouds, and tonemaps with ACES.
//
// Optional features (selected at pipeline-creation time via WGSL
// `override` constants — naga folds the constants and DCEs disabled
// branches, same zero-cost-when-off semantics as C++ `#ifdef`):
//   * `soft_shadow`       — full-precision shadow coord vs. 32-unit
//                           grid-snapped (the C++ default for hard
//                           shadows). Mirrors `SOFT_SHADOW`.
//   * `volumetric_clouds` — 32-iteration cloud raymarch with sun
//                           scattering. Mirrors `VOLUMETRIC_CLOUDS`.
//   * `ambient_occlusion` — 16-sample screen-space SSAO. Mirrors
//                           `AMBIENT_OCCLUSION`.
//
// Skipped vs. the C++ reference (deferred):
//   * Water SSR + wave normals — water alpha-blends in our forward
//     overlay pass and never enters the G-buffer, so the C++ SSR block
//     in `main()` has no equivalent to translate yet.
//   * Cook-Torrance BRDF — the G-buffer doesn't carry metallic /
//     roughness; collapses to Lambert + ambient.
//
// Bind groups:
//   group 0 binding 0 : FrameUniforms                   (uniform buffer)
//   group 1 binding 0 : g_diffuse  : texture_2d<f32>    (Rgba32Float — pre-lit albedo)
//   group 1 binding 1 : g_normal   : texture_2d<f32>    (Rgba8Unorm — encoded normal)
//   group 1 binding 2 : g_material : texture_2d<f32>    (Rgba8Unorm — R/G hold u16 block id)
//   group 1 binding 3 : g_depth    : texture_depth_2d   (Depth32Float — reversed-Z)
//   group 2 binding 0 : shadow_texture : texture_depth_2d
//   group 2 binding 1 : shadow_sampler : sampler_comparison (GreaterEqual)
//   group 2 binding 2 : noise_texture  : texture_2d<f32>
//   group 2 binding 3 : noise_sampler  : sampler

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
var g_diffuse: texture_2d<f32>;
@group(1) @binding(1)
var g_normal: texture_2d<f32>;
@group(1) @binding(2)
var g_material: texture_2d<f32>;
@group(1) @binding(3)
var g_depth: texture_depth_2d;

@group(2) @binding(0)
var shadow_texture: texture_depth_2d;
@group(2) @binding(1)
var shadow_sampler: sampler_comparison;
@group(2) @binding(2)
var noise_texture: texture_2d<f32>;
@group(2) @binding(3)
var noise_sampler: sampler;

// ---- pipeline-creation feature flags ----
//
// Set via `wgpu::PipelineCompilationOptions::constants` when building
// the composition pipeline. naga folds these and dead-code-strips the
// disabled branches, so the cost of an off feature is ~zero at runtime.
override soft_shadow: bool = false;
override volumetric_clouds: bool = false;
override ambient_occlusion: bool = false;

const PI: f32 = 3.141593;

// Sky tint constants — port of C++ `final.fsh::get_sky_color`.
const SKY_HIGH: vec3<f32> = vec3<f32>(0.3, 0.5, 1.2);
const SKY_LOW: vec3<f32> = vec3<f32>(1.2, 1.6, 2.0);
const SUN_RADIANCE: vec3<f32> = vec3<f32>(7.0, 6.0, 5.8);
// = vec3(3.5, 3.0, 2.9) * 2.0
const AMBIENT_RADIANCE: vec3<f32> = vec3<f32>(0.18, 0.25, 0.5);
const EXPOSURE: f32 = 0.6;

// Shadow constants.
const SHADOW_UNITS: f32 = 32.0;

// SSAO constants — match C++ `final.fsh`.
const SSAO_RADIUS: f32 = 1.0;
const SSAO_SAMPLES: i32 = 16;

// Reflective material ids — must match `BaseBlocks` registration order
// in `src/blocks.rs::register_base_blocks`. C++ pins these in
// `final.fsh` lines 38–39 with the same numeric values.
const WATER_ID: u32 = 21u;
const ICE_ID: u32 = 26u;
const IRON_ID: u32 = 28u;

// SSR raymarch constants — match C++ `final.fsh`.
const REFL_ITERATIONS: i32 = 32;
const REFL_STEP_SCALE: f32 = 2.0 / 32.0;

// Water wave constants — match C++ `final.fsh`.
const WAVE_OCTAVES: i32 = 7;
const WAVE_LEVEL: f32 = - 0.5;
const WAVE_SCALE: f32 = 0.01;
const WAVE_MIN_LENGTH: f32 = 4.0;
const WAVE_MAX_LENGTH: f32 = 12.0;
const WAVE_DIRECTION_RANGE: f32 = 0.1;
// Gravity (m/s²) — drives the dispersion relation `c = sqrt(g/k)`.
const WAVE_GRAVITY: f32 = 9.81;

// Volumetric cloud constants — match C++ `final.fsh`.
const NOISE_TEXTURE_SIZE: f32 = 128.0;
const NOISE_TEXTURE_OFFSET: vec2<f32> = vec2<f32>(37.0, 17.0);
const CLOUD_SCALE: vec3<f32> = vec3<f32>(100.0, 80.0, 100.0);
const CLOUD_BOTTOM: f32 = 100.0;
const CLOUD_TOP: f32 = 65536.0;
const CLOUD_TRANSITION: f32 = 120.0;
const CLOUD_ITERATIONS: i32 = 32;
const CLOUD_STEP_SCALE: f32 = 16.0;

struct VsOut {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) uv: vec2<f32>,
}

// Full-screen triangle / quad — six vertices over two triangles.
@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
    let positions = array<vec2<f32>, 6>(vec2<f32>(- 1.0, - 1.0), vec2<f32>(1.0, - 1.0), vec2<f32>(1.0, 1.0), vec2<f32>(- 1.0, - 1.0), vec2<f32>(1.0, 1.0), vec2<f32>(- 1.0, 1.0),);
    let p = positions[vid];
    var out: VsOut;
    out.clip_position = vec4<f32>(p, 0.0, 1.0);
    out.uv = vec2<f32>(p.x * 0.5 + 0.5, 1.0 - (p.y * 0.5 + 0.5));
    return out;
}

// ---- helpers shared by every path ----

fn decode_u16(v: vec2<f32>) -> u32 {
    let hi = u32(v.x * 255.0 + 0.5);
    let lo = u32(v.y * 255.0 + 0.5);
    return hi * 256u + lo;
}

// Hash → `[0, 1)` pseudo-random scalar. Direct port of C++ `rand`.
fn rand2(v: vec2<f32>) -> f32 {
    return fract(sin(dot(v, vec2<f32>(12.9898, 78.233))) * 43758.5453);
}

// Distance from `uv` to the nearest screen edge in `[0, 0.5]`. Used by
// SSAO / SSR to mask out artifacts at viewport boundaries.
fn distance_to_edge(uv: vec2<f32>) -> f32 {
    return min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
}

// Directional sky colour. C++ `frame.sun_dir` would be the direction
// rays travel; our `frame.sun_dir` points TO the sun, hence no negation.
fn get_sky_color(dir: vec3<f32>) -> vec3<f32> {
    let nd = normalize(dir);
    let to_sun = normalize(frame.sun_dir.xyz);
    let tangent = normalize(cross(vec3<f32>(0.0, 1.0, 0.0), to_sun));
    let bitangent = cross(tangent, to_sun);
    let local = vec3<f32>(dot(nd, tangent), dot(nd, bitangent), dot(nd, to_sun));
    if (abs(local.x) < 0.03 && abs(local.y) < 0.03 && local.z > 0.0) {
        return SUN_RADIANCE;
    }
    return mix(SKY_LOW, SKY_HIGH, smoothstep(0.0, 1.0, nd.y * 2.0));
}

// Reconstruct the camera-relative world-space position from a UV + the
// G-buffer depth at that pixel. wgpu NDC y has +Y up, but our `uv`
// follows texture-space (+Y down), so we flip.
//
// `view_proj = proj * view_matrix` (camera-to-origin), so the inverse
// gives ABSOLUTE world position; subtract `camera_pos` to land in
// camera-relative space, which is what the lambert / fog / shadow math
// wants.
fn reconstruct_view_relative(uv: vec2<f32>, depth: f32) -> vec3<f32> {
    let ndc = vec3<f32>(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth);
    let h = frame.inv_view_proj * vec4<f32>(ndc, 1.0);
    let world_pos = h.xyz / h.w;
    return world_pos - frame.camera_pos.xyz;
}

// ---- shadow PCF ----

// Project the player's world position into shadow clip space — the
// fisheye warp's anchor (matches `shadow.wgsl::fisheye_origin`).
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

// 4-tap PCF shadow sample. `textureSampleCompareLevel` (LOD = 0) avoids
// the uniform-control-flow requirement of `textureSampleCompare` —
// callers can branch around this freely.
fn get_shadow_quad(uv: vec2<f32>, ref_d: f32) -> f32 {
    let resolution = max(frame.shadow_params.x, 1.0);
    let texel = 1.0 / resolution;
    var res: f32 = 0.0;
    res += textureSampleCompareLevel(shadow_texture, shadow_sampler, uv + vec2<f32>(- 0.5, - 0.5) * texel, ref_d);
    res += textureSampleCompareLevel(shadow_texture, shadow_sampler, uv + vec2<f32>(0.5, - 0.5) * texel, ref_d);
    res += textureSampleCompareLevel(shadow_texture, shadow_sampler, uv + vec2<f32>(0.5, 0.5) * texel, ref_d);
    res += textureSampleCompareLevel(shadow_texture, shadow_sampler, uv + vec2<f32>(- 0.5, 0.5) * texel, ref_d);
    return res * 0.25;
}

// Sun lighting attenuation factor in `[0, 1]`. Mirrors C++
// `calc_sunlight_radiance_factor`. Branches on `soft_shadow`:
//   * on  — full-precision world coord, normal-bias offset (smooth PCF
//           edge between texels).
//   * off — quantize world coord to a 32-unit grid + normal-bias half a
//           grid cell (eliminates per-fragment shadow noise; a tile of
//           fragments samples the same texel).
fn calc_sunlight_factor(view_relative: vec3<f32>, normal: vec3<f32>) -> f32 {
    let to_sun = normalize(frame.sun_dir.xyz);
    let world_pos = view_relative + frame.camera_pos.xyz;

    let normal_bias = 0.05;
    var biased: vec3<f32>;
    if (soft_shadow) {
        biased = world_pos + to_sun * 0.1 + normal * normal_bias;
    }
    else {
        // Snap to a SHADOW_UNITS grid; mirrors C++ `floor(coord *
        // SHADOW_UNITS + normal * 0.5) / SHADOW_UNITS`. The biased-by-
        // normal half-grid offset keeps the sample a hair off the
        // surface.
        biased = floor(world_pos * SHADOW_UNITS + normal * 0.5) / SHADOW_UNITS + to_sun * 0.1;
    }

    let shadow_clip = frame.shadow_view_proj * vec4<f32>(biased, 1.0);
    let shadow_ndc_pre = shadow_clip.xyz / shadow_clip.w;
    let warped = fisheye_project(shadow_ndc_pre.xy);
    let shadow_ndc = vec3<f32>(warped, shadow_ndc_pre.z);

    let uv = vec2<f32>(shadow_ndc.x * 0.5 + 0.5, 0.5 - shadow_ndc.y * 0.5);
    let uv_clamped = clamp(uv, vec2<f32>(0.0), vec2<f32>(1.0));
    let pcf = get_shadow_quad(uv_clamped, shadow_ndc.z);

    let in_frustum = f32(shadow_ndc.x >= - 1.0 && shadow_ndc.x <= 1.0 && shadow_ndc.y >= - 1.0 && shadow_ndc.y <= 1.0);
    let in_frustum_factor = mix(1.0, pcf, in_frustum);

    let dist = length(view_relative);
    let dist_factor = smoothstep(0.8, 1.0, dist / max(frame.shadow_params.y, 1.0));
    let pcf_with_fade = mix(in_frustum_factor, 1.0, dist_factor);

    let cos_theta = dot(normal, to_sun);
    let facing = step(0.0, cos_theta);
    return pcf_with_fade * facing;
}

// ---- ambient occlusion ----

// 16-sample hemisphere SSAO — direct port of C++ `calc_ambient_factor`.
// Returns ambient attenuation in `[0, 1]` (1 = unoccluded). DCE'd to
// `return 1.0` when `ambient_occlusion` is false.
fn calc_ambient_factor(view_relative: vec3<f32>, normal: vec3<f32>, frag_xy: vec2<f32>) -> f32 {
    if (!ambient_occlusion) {
        return 1.0;
    }
    let world_pos = view_relative + frame.camera_pos.xyz;
    // Build a tangent frame around the surface normal.
    let tangent = normalize(cross(normal, vec3<f32>(1.0, 1.0, 1.0)));
    let bitangent = cross(normal, tangent);

    var res: f32 = 0.0;
    for (var i: i32 = 0; i < SSAO_SAMPLES; i = i + 1) {
        let r = f32(i) / f32(SSAO_SAMPLES);
        let raw_offset = vec3<f32>(rand2(frag_xy + vec2<f32>(r, 0.0)) * 2.0 - 1.0, rand2(frag_xy + vec2<f32>(0.0, r)) * 2.0 - 1.0, rand2(frag_xy + vec2<f32>(r, r)),) * SSAO_RADIUS;
        let sample_world = world_pos + tangent * raw_offset.x + bitangent * raw_offset.y + normal * raw_offset.z;
        let sample_clip = frame.view_proj * vec4<f32>(sample_world, 1.0);
        // Reject samples behind the camera (would project to garbage).
        if (sample_clip.w <= 0.0) {
            res += 1.0;
            continue;
        }
        let sample_ndc = sample_clip.xyz / sample_clip.w;
        let sample_uv = vec2<f32>(sample_ndc.x * 0.5 + 0.5, 0.5 - sample_ndc.y * 0.5);
        let in_bounds = sample_uv.x >= 0.0 && sample_uv.x <= 1.0 && sample_uv.y >= 0.0 && sample_uv.y <= 1.0;
        if (!in_bounds) {
            res += 1.0;
            continue;
        }
        let dim = vec2<f32>(textureDimensions(g_depth));
        let sample_pixel = vec2<i32>(sample_uv * dim);
        let scene_depth = textureLoad(g_depth, sample_pixel, 0);
        // Reversed-Z: larger depth = closer to camera. If the scene at
        // this pixel is closer than the sample point, the sample is
        // BEHIND geometry → occluded. C++ adds a screen-edge fade so
        // SSAO doesn't darken the viewport border.
        if (scene_depth > sample_ndc.z) {
            res += smoothstep(0.8, 1.0, 1.0 - distance_to_edge(sample_uv) * 2.0);
        }
        else {
            res += 1.0;
        }
    }
    return res / f32(SSAO_SAMPLES);
}

// ---- water wave normals ----

// Sum of 7 Gerstner waves over the water surface — direct port of C++
// `final.fsh::calc_wave_normal`. Returns a perturbed surface normal at
// world-space `pos`. The 3-tap height pattern (`hs.x`, `hs.y`, `hs.z`)
// samples three points around `pos.xz` to recover ∂h/∂x and ∂h/∂z by
// finite difference, then crosses them to get the normal.
//
// The fixed-point iteration (`ps0 = ps - a*sin(k*(ps0 + c*t))`) is the
// standard Gerstner-wave inverse-mapping trick — it converts the
// parametric wave displacement back to a height field. Three iterations
// give plenty of precision for `WAVE_LEVEL = -0.5`.
fn calc_wave_normal(pos: vec3<f32>) -> vec3<f32> {
    var hs = vec3<f32>(0.0);
    for (var i: i32 = 0; i < WAVE_OCTAVES; i = i + 1) {
        let ratio = f32(i) / f32(WAVE_OCTAVES);
        let lambda = WAVE_MIN_LENGTH + rand2(vec2<f32>(ratio, ratio)) * (WAVE_MAX_LENGTH - WAVE_MIN_LENGTH);
        let k = 2.0 * PI / lambda;
        let a = exp(k * WAVE_LEVEL) / k;
        let c = sqrt(WAVE_GRAVITY / k);
        let angle = 2.0 * PI * ratio * WAVE_DIRECTION_RANGE;
        let direction = vec2<f32>(cos(angle), sin(angle));
        let ps = vec3<f32>(dot(pos.xz + vec2<f32>(0.1, 0.0), direction), dot(pos.xz, direction), dot(pos.xz + vec2<f32>(0.0, 0.1), direction),);
        // C++ writes `u_game_time / 30.0` to convert its tick counter
        // (30 Hz integer) into seconds. Our `frame.time` is already in
        // seconds, so we use it directly — the C++ /30 was a unit
        // conversion, not a speed scale. Skipping it brings the wave
        // period for an 8-block swell down to ~2.3 s (`λ/c` with
        // `c = √(g·λ/2π)`), close to a real-world ocean swell.
        let t = frame.time;
        var ps0 = ps;
        ps0 = ps - a * sin(k * (ps0 + c * t));
        ps0 = ps - a * sin(k * (ps0 + c * t));
        ps0 = ps - a * sin(k * (ps0 + c * t));
        hs = hs + (- a) * cos(k * (ps0 + c * t));
    }
    hs = hs * WAVE_SCALE;
    let xx = vec3<f32>(0.1, hs.x - hs.y, 0.0);
    let zz = vec3<f32>(0.0, hs.z - hs.y, 0.1);
    return normalize(cross(zz, xx));
}

// ---- screen-space reflections ----

// Schlick Fresnel approximation for a dielectric interface — `n1 / n2`
// are the refractive indices on either side of the surface. Direct port
// of C++ `final.fsh::schlick`. Returns the reflectance fraction in
// `[0, 1]`; the rest is transmitted.
fn schlick(n: f32, m: f32, cos_theta: f32) -> f32 {
    if (cos_theta < 0.0) {
        return 1.0;
    }
    let r0 = pow((n - m) / (n + m), 2.0);
    return r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
}

// NDC `[-1, 1]` xy → texture UV `[0, 1]` with WGSL +Y down.
fn ndc_to_uv(ndc_xy: vec2<f32>) -> vec2<f32> {
    return vec2<f32>(ndc_xy.x * 0.5 + 0.5, 0.5 - ndc_xy.y * 0.5);
}

// G-buffer texture-load helpers used by the SSR raymarch. Bilinear
// filtering doesn't make sense here (we'd average across material
// boundaries) — use `textureLoad` for unfiltered fetches.
fn scene_depth_at(uv: vec2<f32>) -> f32 {
    let dim = vec2<f32>(textureDimensions(g_depth));
    let pixel = vec2<i32>(clamp(uv, vec2<f32>(0.0), vec2<f32>(0.999)) * dim);
    return textureLoad(g_depth, pixel, 0);
}

fn scene_material_at(uv: vec2<f32>) -> u32 {
    let dim = vec2<f32>(textureDimensions(g_material));
    let pixel = vec2<i32>(clamp(uv, vec2<f32>(0.0), vec2<f32>(0.999)) * dim);
    return decode_u16(textureLoad(g_material, pixel, 0).rg);
}

fn scene_normal_at(uv: vec2<f32>) -> vec3<f32> {
    let dim = vec2<f32>(textureDimensions(g_normal));
    let pixel = vec2<i32>(clamp(uv, vec2<f32>(0.0), vec2<f32>(0.999)) * dim);
    return normalize(textureLoad(g_normal, pixel, 0).rgb * 2.0 - vec3<f32>(1.0));
}

fn scene_diffuse_at(uv: vec2<f32>) -> vec3<f32> {
    let dim = vec2<f32>(textureDimensions(g_diffuse));
    let pixel = vec2<i32>(clamp(uv, vec2<f32>(0.0), vec2<f32>(0.999)) * dim);
    return textureLoad(g_diffuse, pixel, 0).rgb;
}

// Unproject a wgpu NDC point (z is reversed-Z `[0, 1]`) back to
// camera-relative world space.
fn unproject_ndc(ndc: vec3<f32>) -> vec3<f32> {
    let h = frame.inv_view_proj * vec4<f32>(ndc, 1.0);
    let world_pos = h.xyz / h.w;
    return world_pos - frame.camera_pos.xyz;
}

// Compute the lit world-space colour AND fog-faded alpha at a UV —
// Lambert + ambient + AO + shadow PCF + distance fog. Direct port of
// C++ `final.fsh::diffuse_with_fog`. The returned alpha bakes in both
// the texel's own opacity (`diffuse.a` — opaque blocks read 1.0,
// water/ice 0.02) and the render-distance horizon fade
// (`clamp((render_dist - dist) / 32, 0, 1)`); callers blend with the
// directional sky via `mix(sky, result.rgb, result.a)`.
//
// Used by both the primary fragment path AND the SSR raymarch's "found"
// hit so reflected pixels get the same shading the camera-direct view
// gets. NOT recursive — skips SSR and the alpha-blend-with-sky step.
// Clouds are applied by callers since they depend on the view direction.
fn shade_world_pixel(uv: vec2<f32>, frag_xy: vec2<f32>) -> vec4<f32> {
    let dim = vec2<f32>(textureDimensions(g_diffuse));
    let pixel = vec2<i32>(clamp(uv, vec2<f32>(0.0), vec2<f32>(0.999)) * dim);
    let depth = textureLoad(g_depth, pixel, 0);
    let view_relative = reconstruct_view_relative(uv, depth);
    let diffuse = textureLoad(g_diffuse, pixel, 0);
    let normal = normalize(textureLoad(g_normal, pixel, 0).rgb * 2.0 - vec3<f32>(1.0));
    let material = decode_u16(textureLoad(g_material, pixel, 0).rg);
    let albedo = diffuse.rgb;

    let to_sun = normalize(frame.sun_dir.xyz);
    let ao = calc_ambient_factor(view_relative, normal, frag_xy);
    let ambient = AMBIENT_RADIANCE * ao;

    var color: vec3<f32>;
    if (material == WATER_ID) {
        color = albedo * ambient;
    }
    else {
        let sun_factor = calc_sunlight_factor(view_relative, normal);
        let cos_n_s = max(dot(normal, to_sun), 0.0);
        let direct = SUN_RADIANCE * (sun_factor * cos_n_s / PI);
        color = albedo * (ambient + direct);
    }

    // Distance fog — fade lit colour toward `SKY_LOW`.
    let dist = length(view_relative);
    let visibility = exp(log(0.9) * dist / max(frame.render_distance, 1.0));
    color = mix(SKY_LOW, color, visibility);
    // Alpha — texel opacity × horizon fade. Mirrors C++
    // `color.a *= clamp((u_render_distance - dist) / 32.0, 0.0, 1.0);`.
    let alpha_fade = clamp((frame.render_distance - dist) / 32.0, 0.0, 1.0);
    let alpha = clamp(diffuse.a * alpha_fade, 0.0, 1.0);
    return vec4<f32>(color, alpha);
}

// Screen-space reflection raymarch — port of C++ `final.fsh::ssr`.
//
// Inputs are clip-space points: `org_clip` is the surface fragment, and
// `dir_clip` is the reflect direction transformed to clip-space (with
// `w = 0` so it's a direction vector, not a point). The raymarch steps
// in NDC, halving the step on first hit to refine, and bails when the
// step shrinks below one pixel.
//
// Returns `(reflected_rgb, valid_alpha)`. Caller blends via
// `mix(sky_reflection, return.rgb, return.a)`.
fn ssr(org_clip: vec4<f32>, dir_clip: vec4<f32>, frag_xy: vec2<f32>) -> vec4<f32> {
    let org3 = org_clip.xyz / org_clip.w;
    let endpoint = org_clip + dir_clip;
    let dir3_unnorm = (endpoint.xyz / endpoint.w) - org3;
    var dir3 = normalize(dir3_unnorm);
    // Normalize so each step covers the same NDC xy distance regardless
    // of the angle to the screen — matches C++ `dir3 /= length(dir3.xy)`.
    let xy_len = length(dir3.xy);
    if (xy_len > 0.0001) {
        dir3 = dir3 / xy_len;
    }

    var step_mult: f32 = 1.0;
    var curr3 = org3;
    var best: vec2<f32> = ndc_to_uv(curr3.xy);
    var found: bool = false;
    var found_ratio: f32 = 1.0;

    let buf_w = max(frame.screen_size.x, 1.0);
    let buf_h = max(frame.screen_size.y, 1.0);

    for (var i: i32 = 0; i < REFL_ITERATIONS; i = i + 1) {
        let ratio = f32(i) / f32(REFL_ITERATIONS);
        var jitter: f32 = 1.0;
        if (i == 0) {
            jitter = 0.5 + cloud_dither(frag_xy);
        }
        let step = step_mult * REFL_STEP_SCALE * jitter;

        // Bail when the refined step is sub-pixel — further iterations
        // can't resolve any new detail.
        if (step_mult * REFL_STEP_SCALE < 2.0 / max(buf_w, buf_h)) {
            break;
        }

        let next3 = curr3 + dir3 * step;
        if (next3.x < - 1.0 || next3.x > 1.0 || next3.y < - 1.0 || next3.y > 1.0) {
            break;
        }

        let tex_coord = ndc_to_uv(next3.xy);
        let z = scene_depth_at(tex_coord);
        // Reversed-Z: `z >= next3.z` ⇔ scene at-or-closer-to-camera than
        // the ray sample → potential intersection. C++ uses the same
        // condition with standard-Z `>=` (sign flips for reversed-Z
        // because both sides flip).
        if (z >= next3.z) {
            if (scene_material_at(tex_coord) != 0u) {
                let sample_ws = unproject_ndc(vec3<f32>(next3.xy, z));
                let curr_ws = unproject_ndc(curr3);
                let surface_normal = scene_normal_at(tex_coord);
                // Reject near-tangent intersections — the dot product is
                // almost zero when the ray grazes a surface, which
                // produces visible streaks.
                if (dot(curr_ws - sample_ws, surface_normal) >= - 0.1) {
                    if (!found) {
                        found_ratio = ratio;
                    }
                    found = true;
                    best = tex_coord;
                }
            }
            step_mult = step_mult * 0.5;
        }
        else {
            curr3 = next3;
        }
    }

    if (!found) {
        return vec4<f32>(0.0);
    }
    // Full lighting + fog + horizon-fade-alpha on the reflected pixel.
    // C++ uses `diffuse_with_fog(best)` here, then multiplies its alpha
    // by an edge-fade factor so the SSR result tapers near screen
    // edges / at the iteration limit. We do the same.
    let lit = shade_world_pixel(best, frag_xy);
    let edge_fade = 1.0 - smoothstep(0.8, 1.0, max(1.0 - distance_to_edge(best) * 2.0, found_ratio),);
    return vec4<f32>(lit.rgb, lit.a * edge_fade);
}

// ---- volumetric clouds ----

// 3D-style noise. C++ does this by indexing the 2D noise atlas with a
// vertical offset to fake a third dimension and lerping between two
// adjacent y-slices. Identical math here, but `Repeat` wrap on
// `noise_sampler` saves us the explicit `mod(uv, NOISE_TEXTURE_SIZE)`.
fn interpolated_noise(x: vec3<f32>) -> f32 {
    let ix = floor(x);
    let fx = fract(x);
    let uv0 = ix.xz + ix.y * NOISE_TEXTURE_OFFSET + fx.xz;
    let uv1 = uv0 + NOISE_TEXTURE_OFFSET;
    let texel0 = textureSampleLevel(noise_texture, noise_sampler, uv0 / NOISE_TEXTURE_SIZE, 0.0).r;
    let texel1 = textureSampleLevel(noise_texture, noise_sampler, uv1 / NOISE_TEXTURE_SIZE, 0.0).r;
    return mix(texel0, texel1, fx.y);
}

fn cloud_noise(c: vec3<f32>) -> f32 {
    var res: f32 = 0.0;
    res += interpolated_noise(c * 1.0);
    res += interpolated_noise(c * 2.0) * 0.5;
    res += interpolated_noise(c * 6.0) * 0.25;
    res += interpolated_noise(c * 24.0) * (1.0 / 12.0);
    return res / (1.0 + 0.5 + 0.25 + 1.0 / 12.0);
}

fn calc_cloud_opacity(pos: vec3<f32>) -> f32 {
    let factor = min(smoothstep(CLOUD_BOTTOM, CLOUD_BOTTOM + CLOUD_TRANSITION, pos.y), 1.0 - smoothstep(CLOUD_TOP - CLOUD_TRANSITION, CLOUD_TOP, pos.y),);
    let opacity = clamp(cloud_noise(pos / CLOUD_SCALE) * 2.0 - 1.2, 0.0, 1.0);
    return sqrt(factor) * opacity;
}

// Per-fragment dither tap into the noise atlas — used to break up the
// cloud raymarch's first-sample stepping artifact. Mirrors C++ `dither`.
//
// The time-dependent offset (C++ `mod(u_game_time, 30.0) *
// NOISE_TEXTURE_OFFSET`) is intentionally suppressed for visual
// debugging — without it the cloud noise pattern is stable across
// frames so the user can A/B-compare without per-frame jitter.
fn cloud_dither(frag_xy: vec2<f32>) -> f32 {
    let v = frag_xy;
    return textureSampleLevel(noise_texture, noise_sampler, v / NOISE_TEXTURE_SIZE, 0.0).b;
}

// 32-step volumetric cloud raymarch — direct port of C++ `cloud()`.
// Returns `(rgb, alpha)` — caller blends via `mix(color, rgb, alpha)`.
//
// * `org` — where the ray starts. Camera position for the primary view,
//   water surface for SSR reflection.
// * `dir` — ray direction (any length; normalized inside).
// * `max_dist` — bail when the ray has walked further than this.
// * `center` — reference point for the render-distance horizon fade.
//   Always the camera position (so reflected clouds dim relative to
//   the player, not relative to the water surface). C++ accepts this
//   as a separate parameter for the same reason.
// * `quality` — step-size divisor. `1.0` for the primary view, `0.5`
//   (= half the steps) for SSR reflection so the reflected raymarch is
//   cheaper.
fn cloud(org: vec3<f32>, dir: vec3<f32>, max_dist: f32, center: vec3<f32>, quality: f32, frag_xy: vec2<f32>,) -> vec4<f32> {
    let nd = normalize(dir);
    let to_sun = normalize(frame.sun_dir.xyz);
    var curr = org;
    var res = vec3<f32>(0.0);
    var remaining: f32 = 1.0;

    // Step the start point onto the cloud layer slab if outside it.
    if (curr.y < CLOUD_BOTTOM) {
        if (nd.y <= 0.0) {
            return vec4<f32>(0.0);
        }
        curr = curr + nd * (CLOUD_BOTTOM - curr.y) / nd.y;
    }
    else if (curr.y > CLOUD_TOP) {
        if (nd.y >= 0.0) {
            return vec4<f32>(0.0);
        }
        curr = curr + nd * (CLOUD_TOP - curr.y) / nd.y;
    }

    let step_base = CLOUD_STEP_SCALE / max(quality, 0.0001);
    for (var i: i32 = 0; i < CLOUD_ITERATIONS; i = i + 1) {
        var step_size = step_base;
        if (i == 0) {
            step_size = step_size * (0.5 + cloud_dither(frag_xy));
        }
        curr = curr + nd * step_size;

        if (remaining < 0.01) {
            break;
        }
        if (length(curr - org) > max_dist) {
            break;
        }
        if (curr.y < CLOUD_BOTTOM || curr.y > CLOUD_TOP) {
            break;
        }

        // Two edge fades:
        //   - distance from `center` (camera) vs render distance — keeps
        //     the cloud field dimming consistently regardless of where
        //     the ray started.
        //   - distance from `org` (ray origin) vs `max_dist` — tapers
        //     the raymarch's tail so it doesn't pop hard at its end.
        let walked = length(curr - org);
        let from_center = length(curr - center);
        var factor: f32 = 1.0;
        factor = factor * (1.0 - smoothstep(frame.render_distance * 0.8, frame.render_distance, from_center));
        factor = factor * (1.0 - smoothstep(max_dist * 0.8, max_dist, walked));
        let transmittance = pow(1.0 - factor * calc_cloud_opacity(curr), step_size);
        if (transmittance < 0.99) {
            // Self-shadow against two sun-direction taps. C++ uses
            // `-u_sunlight_dir` (away from sun); we have `to_sun`
            // directly so we add (toward sun) to walk into the lit
            // side.
            var scattering: f32 = 1.0;
            scattering = scattering * pow(1.0 - calc_cloud_opacity(curr + to_sun * 8.0), 8.0);
            scattering = scattering * pow(1.0 - calc_cloud_opacity(curr + to_sun * 16.0), 8.0);
            let sun_col = vec3<f32>(3.5, 3.0, 2.9);
            let amb_col = vec3<f32>(0.18, 0.25, 0.5);
            let lit = sun_col * scattering + amb_col * (1.0 - scattering);
            res = res + remaining * (1.0 - transmittance) * lit;
            remaining = remaining * transmittance;
        }
    }
    return vec4<f32>(res, 1.0 - remaining);
}

// ACES filmic tonemap.
fn aces(x: vec3<f32>) -> vec3<f32> {
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3<f32>(0.0), vec3<f32>(1.0),);
}

// Anchor the noise binding so wgpu pipeline reflection keeps it live
// when both `volumetric_clouds` and `ambient_occlusion` are off (and
// neither calls into `noise_texture`). Multiplied by 0.0 — visual no-op.
fn anchor_noise_binding(uv: vec2<f32>) -> f32 {
    let n = textureSampleLevel(noise_texture, noise_sampler, uv, 0.0).r;
    return n * 0.0;
}

@fragment
fn fs_main(in: VsOut) -> @location(0) vec4<f32> {
    let pixel = vec2<i32>(in.clip_position.xy);
    let material = decode_u16(textureLoad(g_material, pixel, 0).rg);

    let depth = textureLoad(g_depth, pixel, 0);
    let view_relative = reconstruct_view_relative(in.uv, depth);
    let view_dir = normalize(view_relative);
    let sky = get_sky_color(view_dir);

    var color: vec3<f32>;
    var dist: f32;

    if (material == 0u) {
        // Sky pixel — no fragment was drawn here. Cloud raymarch (if
        // on) goes the full distance to the cloud layer.
        color = sky;
        dist = 65536.0;
    }
    else {
        // Chunk pixel — full lambert + shadow + ambient + fog +
        // texel-opacity-times-horizon-fade alpha via the shared helper
        // that's also called from `ssr()` so reflected pixels get the
        // same shading.
        let lit = shade_world_pixel(in.uv, in.clip_position.xy);
        dist = length(view_relative);

        // Alpha-blend lit chunk colour into the sky tint based on the
        // shaded alpha. Opaque blocks: `lit.a ≈ 1` near camera → no sky;
        // water/ice: `lit.a ≈ 0.02` → mostly sky. Mirrors C++
        // `color = blend(diffuse_with_fog(tex), sky_color)`.
        color = mix(sky, lit.rgb, lit.a);

        // Screen-space reflection on water / ice / iron. Mirrors C++
        // `final.fsh` lines 575–616. Runs even when the player is
        // underwater — only the reflection-base colour and fresnel
        // formula change.
        if (material == WATER_ID || material == ICE_ID || material == IRON_ID) {
            let inside_water = frame.shadow_params.w > 0.5;
            var normal = normalize(textureLoad(g_normal, pixel, 0).rgb * 2.0 - vec3<f32>(1.0));
            let diffuse = textureLoad(g_diffuse, pixel, 0);
            let albedo = diffuse.rgb;
            let view_to_surface = normalize(view_relative);

            // Water wave normal — replace the flat face normal with a
            // 7-octave Gerstner wave perturbation when the surface is
            // mostly horizontal (top of water column). Time-dependent
            // (`frame.time` drives the wave phase). Mirrors C++
            // `final.fsh` lines 580–587. We use `player_coord_mod +
            // player_coord_frac` instead of raw camera_pos so the wave
            // sample point stays in a small numerical range — large
            // world coords kill the trig precision otherwise.
            if (material == WATER_ID && normal.y > 0.9) {
                let surface_world = view_relative + vec3<f32>(frame.player_coord_mod.xyz) + frame.player_coord_frac.xyz;
                let wave_normal = calc_wave_normal(surface_world);
                let to_camera = normalize(- view_relative);
                var cos_check = dot(to_camera, wave_normal);
                if (inside_water) {
                    cos_check = - cos_check;
                }
                if (cos_check >= 0.0) {
                    normal = wave_normal;
                }
            }

            let reflect_dir = reflect(view_to_surface, normal);
            // C++ flips `cos_theta` when inside so the formula below
            // operates on the absolute angle to the surface normal.
            var cos_theta = dot(- view_to_surface, normal);
            if (inside_water) {
                cos_theta = - cos_theta;
            }

            // Reflection base colour. Above water: sky tint (with
            // optional volumetric clouds restored from the C++ TODO).
            // Underwater: a dim grey — C++ `vec3(0.1)` — because the
            // reflection direction points further into the underwater
            // scene and the sky isn't directly visible through that
            // ray.
            var reflection: vec3<f32>;
            if (inside_water) {
                reflection = vec3<f32>(0.1);
            }
            else {
                reflection = get_sky_color(reflect_dir);
                if (volumetric_clouds) {
                    // Cloud raymarch in the reflection direction.
                    // `org` is the WATER SURFACE (the ray's true start
                    // point), not the camera — otherwise the raymarch
                    // would walk through clouds between camera and
                    // water before bouncing, producing wrong cloud
                    // distances. `center` stays at the camera so the
                    // render-distance horizon fade is consistent with
                    // the primary view. `quality = 0.5` halves the
                    // step count for cheaper reflection sampling
                    // (matches C++ `cloud(..., 0.5)`).
                    let surface_world = view_relative + frame.camera_pos.xyz;
                    let refl_clouds = cloud(surface_world, reflect_dir, 65536.0, frame.camera_pos.xyz, 0.5, in.clip_position.xy);
                    reflection = mix(reflection, refl_clouds.rgb, refl_clouds.a);
                }
            }

            // SSR raymarch in clip space.
            let chunk_ndc = vec3<f32>(in.uv.x * 2.0 - 1.0, 1.0 - in.uv.y * 2.0, depth);
            let org_clip = vec4<f32>(chunk_ndc, 1.0);
            let dir_clip = frame.view_proj * vec4<f32>(reflect_dir, 0.0);
            let ssr_result = ssr(org_clip, dir_clip, in.clip_position.xy);
            reflection = mix(reflection, ssr_result.rgb, ssr_result.a);

            // Fresnel / heuristic mix factor.
            //   Above water: physically-based Schlick on water/ice IORs.
            //   Underwater:  `smoothstep(0, 1, sin²θ)` — C++'s heuristic
            //                stand-in for total internal reflection. At
            //                normal incidence (looking straight up) →
            //                near 0 (transparent); at grazing angles →
            //                near 1 (full TIR).
            //                TODO: replace with a better TIR
            //                approximation if available — the
            //                smoothstep makes distant horizontal views
            //                opaque, which hides terrain at grazing
            //                angles.
            //   Iron: fully reflective (fresnel = 1) tinted by surface
            //         diffuse — matches C++.
            var fresnel: f32 = 1.0;
            if (inside_water) {
                fresnel = smoothstep(0.0, 1.0, 1.0 - cos_theta * cos_theta);
            }
            else if (material == WATER_ID) {
                fresnel = schlick(1.0, 1.33, cos_theta);
            }
            else if (material == ICE_ID) {
                fresnel = schlick(1.0, 2.42, cos_theta);
            }
            else {
                reflection = reflection * albedo * 0.5;
            }
            color = mix(color, reflection, fresnel);
        }
    }

    // Volumetric clouds — blend over both sky and chunk paths so cloud
    // shadows on the world land in the same compositing step. Skipped
    // (DCE'd) when the override is off. Primary-view raymarch: `org`
    // and `center` are both the camera (consistent fade); `quality =
    // 1.0` for full step count.
    if (volumetric_clouds) {
        let cloud_result = cloud(frame.camera_pos.xyz, view_dir, dist, frame.camera_pos.xyz, 1.0, in.clip_position.xy);
        color = mix(color, cloud_result.rgb, cloud_result.a);
    }

    // Anchor noise binding when both cloud + AO are off so wgpu doesn't
    // dead-strip the binding from the shader's reflection (the
    // composition pipeline layout always includes noise).
    color = color + vec3<f32>(anchor_noise_binding(in.uv));

    return vec4<f32>(aces(color * EXPOSURE), 1.0);
}

// Deferred composition pass — reads the two-layer G-buffer (opaque +
// translucent) and writes the surface.
//
// Both fragment entries (`fs_main_basic` and `fs_main_advanced`) sample
// both layers and manually compose: front-most translucent over
// shaded opaque over sky. Translucent's own depth attachment kept the
// front-most translucent fragment; the chunk shader's "discard if
// behind opaque" guard ensures translucent.depth > 0 only where the
// translucent fragment is in front of opaque (or where there is no
// opaque). Composition therefore reduces to:
//
//   if neither layer drew → sky
//   else if only opaque   → opaque shaded
//   else if only translucent → translucent shaded over sky
//   else                   → translucent shaded over opaque shaded
//
// Optional advanced features (selected at pipeline-creation time via
// WGSL `override` constants — naga folds the values and DCEs disabled
// branches, so an off feature has ~zero runtime cost):
//   * `soft_shadow`       — full-precision shadow coord vs. 32-unit
//                           grid-snapped (the default for hard shadows).
//   * `volumetric_clouds` — 32-iteration cloud raymarch with sun
//                           scattering.
//   * `ambient_occlusion` — 16-sample screen-space SSAO.
//
// Bind groups (advanced):
//   group 0 binding 0 : FrameUniforms                    (uniform buffer)
//   group 1           : opaque G-buffer
//     binding 0       : g_o_diffuse  : texture_2d<f32>     (Rgba16Float)
//     binding 1       : g_o_normal   : texture_2d<f32>     (Rg8Unorm — octahedral)
//     binding 2       : g_o_material : texture_2d<u32>     (R16Uint — atlas layer)
//     binding 3       : g_o_depth    : texture_depth_2d    (Depth32Float)
//   group 2           : translucent G-buffer (same layout as opaque)
//     binding 0       : g_t_diffuse  : texture_2d<f32>
//     binding 1       : g_t_normal   : texture_2d<f32>
//     binding 2       : g_t_material : texture_2d<u32>
//     binding 3       : g_t_depth    : texture_depth_2d
//   group 3           : aux (advanced only)
//     binding 0       : shadow_texture : texture_depth_2d
//     binding 1       : shadow_sampler : sampler_comparison
//     binding 2       : noise_texture  : texture_2d<f32>
//     binding 3       : noise_sampler  : sampler
//
// Bind groups (basic): same group 1 / group 2 layouts but only
// `binding 0` and `binding 3` per group (diffuse + depth) are
// declared in the host-side bind-group layout — `fs_main_basic`
// references only those.

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
    material_layers: vec4<u32>,
    shadow_params: vec4<f32>,
    player_coord_int: vec4<i32>,
    player_coord_mod: vec4<i32>,
    player_coord_frac: vec4<f32>,
}

@group(0) @binding(0)
var<uniform> frame: FrameUniforms;

// --- group 1 : opaque layer ---
@group(1) @binding(0)
var g_o_diffuse: texture_2d<f32>;
@group(1) @binding(1)
var g_o_normal: texture_2d<f32>;
@group(1) @binding(2)
var g_o_material: texture_2d<u32>;
@group(1) @binding(3)
var g_o_depth: texture_depth_2d;

// --- group 2 : translucent layer ---
@group(2) @binding(0)
var g_t_diffuse: texture_2d<f32>;
@group(2) @binding(1)
var g_t_normal: texture_2d<f32>;
@group(2) @binding(2)
var g_t_material: texture_2d<u32>;
@group(2) @binding(3)
var g_t_depth: texture_depth_2d;

// --- group 3 : advanced aux ---
@group(3) @binding(0)
var shadow_texture: texture_depth_2d;
@group(3) @binding(1)
var shadow_sampler: sampler_comparison;
@group(3) @binding(2)
var noise_texture: texture_2d<f32>;
@group(3) @binding(3)
var noise_sampler: sampler;

override soft_shadow: bool = false;
override volumetric_clouds: bool = false;
override ambient_occlusion: bool = false;

const PI: f32 = 3.141593;

// Time-of-day sky palette — three keyframes (day, dusk, night). See
// `sky_palette()` for the elevation lerp.
const DAY_HIGH: vec3<f32> = vec3<f32>(0.3, 0.5, 1.2);
const DAY_LOW: vec3<f32> = vec3<f32>(1.2, 1.6, 2.0);
const DUSK_HIGH: vec3<f32> = vec3<f32>(0.25, 0.18, 0.45);
const DUSK_LOW: vec3<f32> = vec3<f32>(2.4, 1.0, 0.35);
const NIGHT_HIGH: vec3<f32> = vec3<f32>(0.015, 0.025, 0.06);
const NIGHT_LOW: vec3<f32> = vec3<f32>(0.04, 0.06, 0.12);
const SUN_RADIANCE: vec3<f32> = vec3<f32>(7.0, 6.0, 5.8);
const EXPOSURE: f32 = 0.6;

// Warm white for emissive (block-light) sources.
const BLOCK_LIGHT_TINT: vec3<f32> = vec3<f32>(1.0, 0.85, 0.65);

const SHADOW_UNITS: f32 = 32.0;
const SSAO_RADIUS: f32 = 1.0;
const SSAO_SAMPLES: i32 = 16;

// Water wave constants — direct port of `final.fsh`.
const WAVE_OCTAVES: i32 = 7;
const WAVE_LEVEL: f32 = - 0.5;
const WAVE_SCALE: f32 = 0.01;
const WAVE_MIN_LENGTH: f32 = 4.0;
const WAVE_MAX_LENGTH: f32 = 12.0;
const WAVE_DIRECTION_RANGE: f32 = 0.1;
const WAVE_GRAVITY: f32 = 9.81;

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

@vertex
fn vs_main(@builtin(vertex_index) vid: u32) -> VsOut {
    let positions = array<vec2<f32>, 6>(vec2<f32>(- 1.0, - 1.0), vec2<f32>(1.0, - 1.0), vec2<f32>(1.0, 1.0), vec2<f32>(- 1.0, - 1.0), vec2<f32>(1.0, 1.0), vec2<f32>(- 1.0, 1.0),);
    let p = positions[vid];
    var out: VsOut;
    out.clip_position = vec4<f32>(p, 0.0, 1.0);
    out.uv = vec2<f32>(p.x * 0.5 + 0.5, 1.0 - (p.y * 0.5 + 0.5));
    return out;
}

// ---- shared helpers ----

fn rand2(v: vec2<f32>) -> f32 {
    return fract(sin(dot(v, vec2<f32>(12.9898, 78.233))) * 43758.5453);
}

fn distance_to_edge(uv: vec2<f32>) -> f32 {
    return min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));
}

fn reconstruct_view_relative(uv: vec2<f32>, depth: f32) -> vec3<f32> {
    let ndc = vec3<f32>(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth);
    let h = frame.inv_view_proj * vec4<f32>(ndc, 1.0);
    let world_pos = h.xyz / h.w;
    return world_pos - frame.camera_pos.xyz;
}

// Octahedral decode — inverse of `oct_encode_unorm` in chunk.wgsl.
fn oct_decode(stored: vec2<f32>) -> vec3<f32> {
    let p = stored * 2.0 - vec2<f32>(1.0);
    var n = vec3<f32>(p.x, p.y, 1.0 - abs(p.x) - abs(p.y));
    if (n.z < 0.0) {
        let s = vec2<f32>(select(- 1.0, 1.0, n.x >= 0.0), select(- 1.0, 1.0, n.y >= 0.0));
        let v = (1.0 - abs(vec2<f32>(n.y, n.x))) * s;
        n.x = v.x;
        n.y = v.y;
    }
    return normalize(n);
}

// ---- sky ----

struct SkyPalette {
    high: vec3<f32>,
    low: vec3<f32>,
}

fn sky_palette() -> SkyPalette {
    let elevation = frame.sun_dir.y;
    var p: SkyPalette;
    if (elevation >= 0.3) {
        p.high = DAY_HIGH;
        p.low = DAY_LOW;
    }
    else if (elevation >= 0.0) {
        let t = elevation / 0.3;
        p.high = mix(DUSK_HIGH, DAY_HIGH, t);
        p.low = mix(DUSK_LOW, DAY_LOW, t);
    }
    else if (elevation >= - 0.2) {
        let t = (elevation + 0.2) / 0.2;
        p.high = mix(NIGHT_HIGH, DUSK_HIGH, t);
        p.low = mix(NIGHT_LOW, DUSK_LOW, t);
    }
    else {
        p.high = NIGHT_HIGH;
        p.low = NIGHT_LOW;
    }
    return p;
}

fn sky_gradient_color(dir: vec3<f32>) -> vec3<f32> {
    let nd = normalize(dir);
    let p = sky_palette();
    return mix(p.low, p.high, smoothstep(0.0, 1.0, nd.y * 2.0));
}

// Time-of-day scalars used by the lambert / ambient / cloud paths.
//
// * `sun_radiance` — `SUN_RADIANCE` ramped by sun elevation through
//   a thin smoothstep band centred on `sun_dir.y == 0`. Direct
//   sunlight is "all or nothing": once the sun is up surfaces
//   receive the full irradiance regardless of elevation; the
//   smoothstep just avoids a hard cutoff that would pop at sunrise
//   / sunset.
// * `ambient_radiance` — the world's diffuse skylight, derived from
//   the current sky palette so it transitions in lockstep with the
//   day → dusk → night sky colour. Returns `sky_palette().high *
//   0.75` — three-quarters of the zenith colour. At dusk it warms;
//   at night it sinks to the night palette's zenith × 0.75 so
//   caves stay faintly tinted instead of going black.

fn sun_radiance() -> vec3<f32> {
    return smoothstep(- 0.2, 0.2, frame.sun_dir.y) * SUN_RADIANCE;
}

fn ambient_radiance() -> vec3<f32> {
    return sky_palette().high * 0.75;
}

fn get_sky_color(dir: vec3<f32>) -> vec3<f32> {
    let nd = normalize(dir);
    let to_sun = normalize(frame.sun_dir.xyz);
    let tangent = normalize(cross(vec3<f32>(0.0, 1.0, 0.0), to_sun));
    let bitangent = cross(tangent, to_sun);
    let local = vec3<f32>(dot(nd, tangent), dot(nd, bitangent), dot(nd, to_sun));
    if (abs(local.x) < 0.03 && abs(local.y) < 0.03 && local.z > 0.0) {
        return SUN_RADIANCE;
    }
    return sky_gradient_color(nd);
}

// ---- water wave normals ----

// 7-octave Gerstner-wave perturbation — direct port of
// `final.fsh::calc_wave_normal`. Three-tap height pattern (`hs.x`,
// `hs.y`, `hs.z`) samples three points around `pos.xz` to recover
// ∂h/∂x and ∂h/∂z by finite difference, then crosses them to get
// the normal. The fixed-point iteration solves the parametric →
// height mapping for each Gerstner wave; three iterations are
// enough for `WAVE_LEVEL = -0.5`.
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
        // `frame.time` is already in seconds; the C++ shader divides
        // its 30 Hz tick counter by 30 for the same effect.
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

// ---- shadow PCF (advanced only) ----

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

fn calc_sunlight_factor(view_relative: vec3<f32>, normal: vec3<f32>) -> f32 {
    let to_sun = normalize(frame.sun_dir.xyz);
    let world_pos = view_relative + frame.camera_pos.xyz;

    let normal_bias = 0.05;
    var biased: vec3<f32>;
    if (soft_shadow) {
        biased = world_pos + to_sun * 0.1 + normal * normal_bias;
    }
    else {
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

// ---- SSAO (advanced only) ----

fn calc_ambient_factor(view_relative: vec3<f32>, normal: vec3<f32>, frag_xy: vec2<f32>) -> f32 {
    if (!ambient_occlusion) {
        return 1.0;
    }
    let world_pos = view_relative + frame.camera_pos.xyz;
    let tangent = normalize(cross(normal, vec3<f32>(1.0, 1.0, 1.0)));
    let bitangent = cross(normal, tangent);

    var res: f32 = 0.0;
    for (var i: i32 = 0; i < SSAO_SAMPLES; i = i + 1) {
        let r = f32(i) / f32(SSAO_SAMPLES);
        let raw_offset = vec3<f32>(rand2(frag_xy + vec2<f32>(r, 0.0)) * 2.0 - 1.0, rand2(frag_xy + vec2<f32>(0.0, r)) * 2.0 - 1.0, rand2(frag_xy + vec2<f32>(r, r)),) * SSAO_RADIUS;
        let sample_world = world_pos + tangent * raw_offset.x + bitangent * raw_offset.y + normal * raw_offset.z;
        let sample_clip = frame.view_proj * vec4<f32>(sample_world, 1.0);
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
        let dim = vec2<f32>(textureDimensions(g_o_depth));
        let sample_pixel = vec2<i32>(sample_uv * dim);
        let scene_depth = textureLoad(g_o_depth, sample_pixel, 0);
        if (scene_depth > sample_ndc.z) {
            res += smoothstep(0.8, 1.0, 1.0 - distance_to_edge(sample_uv) * 2.0);
        }
        else {
            res += 1.0;
        }
    }
    return res / f32(SSAO_SAMPLES);
}

// ---- volumetric clouds (advanced only) ----

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

fn cloud_dither(frag_xy: vec2<f32>) -> f32 {
    return textureSampleLevel(noise_texture, noise_sampler, frag_xy / NOISE_TEXTURE_SIZE, 0.0).b;
}

fn cloud(org: vec3<f32>, dir: vec3<f32>, max_dist: f32, frag_xy: vec2<f32>) -> vec4<f32> {
    let nd = normalize(dir);
    let to_sun = normalize(frame.sun_dir.xyz);
    var curr = org;
    var res = vec3<f32>(0.0);
    var remaining: f32 = 1.0;

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

    let step_base = CLOUD_STEP_SCALE;
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

        let walked = length(curr - org);
        var factor: f32 = 1.0;
        factor = factor * (1.0 - smoothstep(frame.render_distance * 0.8, frame.render_distance, walked));
        factor = factor * (1.0 - smoothstep(max_dist * 0.8, max_dist, walked));
        let transmittance = pow(1.0 - factor * calc_cloud_opacity(curr), step_size);
        if (transmittance < 0.99) {
            // Self-shadow against two sun-direction taps. Direct port
            // of `final.fsh`: scattering = ∏ pow(1 - opacity, 8) at
            // (curr + to_sun · 8) and (curr + to_sun · 16). Cloud
            // colour is sun radiance when scattering ≈ 1, ambient
            // when scattering ≈ 0.
            let s1 = pow(1.0 - calc_cloud_opacity(curr + to_sun * 8.0), 8.0);
            let s2 = pow(1.0 - calc_cloud_opacity(curr + to_sun * 16.0), 8.0);
            let scattering = s1 * s2;
            // Time-of-day-aware cloud colour: sun-scattered side fades
            // to 0 at night; ambient-shadowed side fades less so dark
            // clouds remain faintly visible against the night sky.
            let cloud_color = sun_radiance() * scattering * 0.5 + ambient_radiance();
            res = res + cloud_color * (1.0 - transmittance) * remaining;
            remaining = remaining * transmittance;
        }
    }
    return vec4<f32>(res, 1.0 - remaining);
}

fn aces(x: vec3<f32>) -> vec3<f32> {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), vec3<f32>(0.0), vec3<f32>(1.0));
}

fn anchor_noise_binding(uv: vec2<f32>) -> f32 {
    return textureSampleLevel(noise_texture, noise_sampler, uv, 0.0).r * 0.0;
}

// ---- per-layer shading (advanced) ----

// Shade an opaque-layer pixel — full lambert + shadow PCF + ambient
// (with optional SSAO) + emissive (from diffuse.a) + distance fog
// into the sky gradient. Returns the un-tonemapped HDR colour.
// `skip_direct` zeroes the lambert sun term — used by SSR-refraction
// hits, where the opaque "underwater terrain" geometry shouldn't
// receive any direct sun (water absorbs it before it reaches the
// bottom). Ambient + emissive still apply; the result reads as
// terrain lit by skylight only.
fn shade_opaque_advanced(pixel: vec2<i32>, frag_xy: vec2<f32>, skip_direct: bool) -> vec3<f32> {
    let depth = textureLoad(g_o_depth, pixel, 0);
    // Reconstruct the centered-pixel UV from the integer pixel
    // coordinate. (Used only by `reconstruct_view_relative` — every
    // other texture access already takes `pixel` directly.)
    let dim = vec2<f32>(textureDimensions(g_o_depth));
    let uv = (vec2<f32>(pixel) + vec2<f32>(0.5)) / dim;
    let view_relative = reconstruct_view_relative(uv, depth);
    let diffuse = textureLoad(g_o_diffuse, pixel, 0);
    let albedo = diffuse.rgb;
    let emissive_intensity = diffuse.a;
    let normal_texel = textureLoad(g_o_normal, pixel, 0);
    let normal = oct_decode(normal_texel.rg);
    // Per-vertex sky-light intensity packed in normal.b — used to
    // attenuate albedo.
    let sky_visibility = normal_texel.b;

    let to_sun = normalize(frame.sun_dir.xyz);
    let ao = calc_ambient_factor(view_relative, normal, frag_xy);
    let ambient = ambient_radiance() * ao;

    var direct = vec3<f32>(0.0);
    if (!skip_direct) {
        let sun_factor = calc_sunlight_factor(view_relative, normal);
        let cos_n_s = max(dot(normal, to_sun), 0.0);
        direct = sun_radiance() * (sun_factor * cos_n_s / PI);
    }
    let emissive = albedo * emissive_intensity * BLOCK_LIGHT_TINT;

    var color = albedo * (ambient + direct) * sky_visibility + emissive;

    let dist = length(view_relative);
    let visibility = exp(log(0.9) * dist / max(frame.render_distance, 1.0));
    let sky_at_pixel = sky_gradient_color(normalize(view_relative));
    color = mix(sky_at_pixel, color, visibility);
    return color;
}

// Shade a translucent-layer pixel. Same lambert + shadow + ambient
// path as opaque but no emissive (translucent surfaces don't emit
// per the G-buffer contract). Only called for non-SSR translucents
// (leaves, glass) — water and ice take the energy-conserving R/T
// path in `fs_main_advanced` and bypass this function entirely.
// Returns un-tonemapped HDR colour with the texel alpha for compositing.
fn shade_translucent_advanced(pixel: vec2<i32>, frag_xy: vec2<f32>) -> vec4<f32> {
    let depth = textureLoad(g_t_depth, pixel, 0);
    let dim = vec2<f32>(textureDimensions(g_t_depth));
    let uv = (vec2<f32>(pixel) + vec2<f32>(0.5)) / dim;
    let view_relative = reconstruct_view_relative(uv, depth);
    let diffuse = textureLoad(g_t_diffuse, pixel, 0);
    let albedo = diffuse.rgb;
    let alpha = diffuse.a;
    let normal_texel = textureLoad(g_t_normal, pixel, 0);
    let normal = oct_decode(normal_texel.rg);
    let sky_visibility = normal_texel.b;

    let to_sun = normalize(frame.sun_dir.xyz);
    let ao = calc_ambient_factor(view_relative, normal, frag_xy);
    let ambient = ambient_radiance() * ao;

    let sun_factor = calc_sunlight_factor(view_relative, normal);
    let cos_n_s = max(dot(normal, to_sun), 0.0);
    let direct = sun_radiance() * (sun_factor * cos_n_s / PI) * sky_visibility;

    var color = albedo * (ambient + direct);

    let dist = length(view_relative);
    let visibility = exp(log(0.9) * dist / max(frame.render_distance, 1.0));
    let sky_at_pixel = sky_gradient_color(normalize(view_relative));
    color = mix(sky_at_pixel, color, visibility);
    return vec4<f32>(color, alpha);
}

// ---- screen-space reflection (advanced only) ----

const REFL_ITERATIONS: i32 = 32;
const REFL_STEP_SCALE: f32 = 2.0 / 32.0;

// Schlick Fresnel approximation for a dielectric interface — `n` /
// `m` are the indices of refraction on either side of the surface.
// Returns the reflectance fraction in `[0, 1]`. Below grazing
// (`cos_theta < 0`, viewing from behind) returns 1 to avoid spurious
// negative powers.
fn schlick(n: f32, m: f32, cos_theta: f32) -> f32 {
    if (cos_theta < 0.0) {
        return 1.0;
    }
    let r0 = pow((n - m) / (n + m), 2.0);
    return r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
}

fn ndc_to_uv(ndc_xy: vec2<f32>) -> vec2<f32> {
    return vec2<f32>(ndc_xy.x * 0.5 + 0.5, 0.5 - ndc_xy.y * 0.5);
}

// Screen-space reflection raymarch — direct port of `final.fsh::ssr`.
// Returns `vec4(rgb, valid)`:
// * `rgb`   — fully shaded opaque colour at the hit pixel (run through
//             the same lambert + shadow + ambient + emissive + fog
//             path the primary view uses).
// * `valid` — `1` near the hit, fading to `0` toward screen edges +
//             early hits along the ray (matches the C++ edge-fade so
//             SSR misses don't pop hard at the viewport border or
//             where the raymarch ran out of refinement budget).
//
// On miss returns `vec4(0.0)`; the caller blends this against a base
// reflection (sky outside / dim grey when underwater).
//
// `frag_xy` is used (a) as the SSAO/shadow seed at the hit pixel and
// (b) as the dither seed for the first-iteration step jitter.
fn unproject_ndc(ndc: vec3<f32>) -> vec3<f32> {
    let h = frame.inv_view_proj * vec4<f32>(ndc, 1.0);
    let world_pos = h.xyz / h.w;
    return world_pos - frame.camera_pos.xyz;
}

// `skip_direct_at_hit` is forwarded to `shade_opaque_advanced` for
// the hit pixel. False for reflection rays (the reflected geometry
// is dry land seen via the water surface — sun applies). True for
// refraction rays (the hit is underwater terrain — sun absorbed by
// the water column).
//
// `normal_reject` toggles the false-positive filter:
// * **true** (reflections) — discards candidate hits where the ray
//   hit the surface at ~grazing tangency (the C++ filter via
//   `dot(curr - sample, hit_normal) >= -0.1`). Reflections look
//   wrong if a tangent hit is taken as the bounce point.
// * **false** (refractions) — accept any depth-passing hit. Stricter
//   filtering for refraction creates *holes* in the underwater image
//   wherever foreground geometry occludes the bent ray; the
//   cascade-into-reflection behaviour means these holes register as
//   "the surface looks reflective" instead of "the surface returns
//   refracted terrain", which reads as a fake mirror patch in the
//   middle of the water. With `normal_reject = false` the refracted
//   ray returns whatever it hit, giving a continuous underwater
//   image; the only true miss is the ray exiting NDC, which the
//   `break` handles and the edge_fade smooths.
fn ssr(org_clip: vec4<f32>, dir_clip: vec4<f32>, frag_xy: vec2<f32>, skip_direct_at_hit: bool, normal_reject: bool,) -> vec4<f32> {
    let org3 = org_clip.xyz / org_clip.w;
    let endpoint = org_clip + dir_clip;
    let dir3_unnorm = (endpoint.xyz / endpoint.w) - org3;
    var dir3 = normalize(dir3_unnorm);
    // Normalize so each step covers the same NDC xy distance regardless
    // of the angle to the screen — matches `final.fsh::dir3 /=
    // length(dir3.xy)`.
    let xy_len = length(dir3.xy);
    if (xy_len > 0.0001) {
        dir3 = dir3 / xy_len;
    }

    var step_mult: f32 = 1.0;
    var curr3 = org3;
    var found: bool = false;
    var found_ratio: f32 = 1.0;
    var hit_pixel: vec2<i32> = vec2<i32>(0, 0);

    let buf_w = max(frame.screen_size.x, 1.0);
    let buf_h = max(frame.screen_size.y, 1.0);
    let dim = vec2<f32>(textureDimensions(g_o_depth));

    // `prev_pixel` tracks the most recent **no-hit** screen pixel
    // along the ray. When a hit is registered we use `prev_pixel`
    // (NOT the just-hit pixel) as the shading sample. Rationale:
    // the hit step's pixel is whatever opaque fragment the ray
    // bumped into — for refraction that's often a foreground
    // occluder (e.g. on-shore terrain whose depth is far closer
    // to the camera than the underwater target the refracted ray
    // was aiming for). The previous-step pixel is the one the ray
    // walked through cleanly just before being intercepted, which
    // for refraction is far more likely to be the actual underwater
    // target. Initialised to the surface (ray-origin) pixel so a
    // first-iteration hit still has something sensible to shade.
    let initial_uv = ndc_to_uv(org3.xy);
    var prev_pixel: vec2<i32> = vec2<i32>(clamp(initial_uv, vec2<f32>(0.0), vec2<f32>(1.0 - 1e-4)) * dim);

    for (var i: i32 = 0; i < REFL_ITERATIONS; i = i + 1) {
        // Bail when refined step is sub-pixel — further iterations
        // can't resolve any new detail.
        if (step_mult * REFL_STEP_SCALE < 2.0 / max(buf_w, buf_h)) {
            break;
        }
        let ratio = f32(i) / f32(REFL_ITERATIONS);
        var jitter: f32 = 1.0;
        if (i == 0) {
            jitter = 0.5 + cloud_dither(frag_xy);
        }
        let step = step_mult * REFL_STEP_SCALE * jitter;
        let next3 = curr3 + dir3 * step;
        let uv = ndc_to_uv(next3.xy);
        let pixel = vec2<i32>(clamp(uv, vec2<f32>(0.0), vec2<f32>(1.0 - 1e-4)) * dim);

        var accept = false;
        if (next3.x >= - 1.0 && next3.x <= 1.0 && next3.y >= - 1.0 && next3.y <= 1.0) {
            let z = textureLoad(g_o_depth, pixel, 0);
            if (z >= next3.z && z > 0.0) {
                // Normal-rejection filter (reflections only): when
                // the ray grazes a surface tangentially, the
                // reported hit's normal points roughly along the
                // ray direction and the pixel reflection is bogus.
                // Reject hits where the angle between (curr→sample)
                // and the hit normal is larger than ~95° (cos <
                // -0.1). Direct port of the C++ check. Refractions
                // skip this so foreground occluders don't carve
                // holes in the underwater image.
                if (normal_reject) {
                    let sample_ws = unproject_ndc(vec3<f32>(next3.xy, z));
                    let curr_ws = unproject_ndc(curr3);
                    let surface_normal = oct_decode(textureLoad(g_o_normal, pixel, 0).rg);
                    accept = dot(curr_ws - sample_ws, surface_normal) >= - 0.1;
                }
                else {
                    accept = true;
                }
            }
        }
        else {
            // NDC-out-of-bounds-rejection filter.
            if (normal_reject) {
                break;
            }
            else {
                accept = true;
            }
        }

        if (accept) {
            if (!found) {
                found = true;
                found_ratio = ratio;
            }
            step_mult = step_mult * 0.5;
            // Use the previous (pre-hit) pixel for shading
            // — see `prev_pixel`'s declaration comment.
            hit_pixel = prev_pixel;
        }
        else {
            curr3 = next3;
            // Walked past this pixel without a hit — record it so
            // the next iteration's hit can sample here instead of
            // the obstructing fragment that caused the hit.
            prev_pixel = pixel;
        }
    }

    if (!found) {
        return vec4<f32>(0.0);
    }

    // Edge fade: reduce contribution near the screen border + when
    // the hit was in the early (under-refined) iterations.
    var edge_fade = 1.0;
    if (normal_reject) {
        let hit_uv = (vec2<f32>(hit_pixel) + vec2<f32>(0.5)) / dim;
        edge_fade = 1.0 - smoothstep(0.8, 1.0, max(1.0 - distance_to_edge(hit_uv) * 2.0, found_ratio),);
    }
    let lit = shade_opaque_advanced(hit_pixel, frag_xy, skip_direct_at_hit);
    return vec4<f32>(lit, edge_fade);
}

// ---- entry points ----

@fragment
fn fs_main_basic(in: VsOut) -> @location(0) vec4<f32> {
    let pixel = vec2<i32>(in.clip_position.xy);
    let opaque_d = textureLoad(g_o_depth, pixel, 0);
    let translucent_d = textureLoad(g_t_depth, pixel, 0);

    // View ray for sky lookup.
    let ndc_far = vec3<f32>(in.uv.x * 2.0 - 1.0, 1.0 - in.uv.y * 2.0, 0.0);
    let world_h = frame.inv_view_proj * vec4<f32>(ndc_far, 1.0);
    let view_dir = normalize((world_h.xyz / world_h.w) - frame.camera_pos.xyz);
    let sky_full_tm = aces(get_sky_color(view_dir) * EXPOSURE);

    // Background = opaque (with fog into sky gradient) or sky if no opaque.
    var background = sky_full_tm;
    if (opaque_d > 0.0) {
        let view_relative = reconstruct_view_relative(in.uv, opaque_d);
        let diffuse = textureLoad(g_o_diffuse, pixel, 0).rgb;
        let dist = length(view_relative);
        let visibility = exp(log(0.9) * dist / max(frame.render_distance, 1.0));
        let sky_gradient_tm = aces(sky_gradient_color(normalize(view_relative)) * EXPOSURE);
        background = mix(sky_gradient_tm, diffuse, visibility);
    }

    // Translucent layer over background.
    if (translucent_d > 0.0) {
        let view_relative = reconstruct_view_relative(in.uv, translucent_d);
        let t_diffuse = textureLoad(g_t_diffuse, pixel, 0);
        let dist = length(view_relative);
        let visibility = exp(log(0.9) * dist / max(frame.render_distance, 1.0));
        let sky_gradient_tm = aces(sky_gradient_color(normalize(view_relative)) * EXPOSURE);
        let t_with_fog = mix(sky_gradient_tm, t_diffuse.rgb, visibility);
        return vec4<f32>(mix(background, t_with_fog, t_diffuse.a), 1.0);
    }
    return vec4<f32>(background, 1.0);
}

@fragment
fn fs_main_advanced(in: VsOut) -> @location(0) vec4<f32> {
    let pixel = vec2<i32>(in.clip_position.xy);
    let opaque_d = textureLoad(g_o_depth, pixel, 0);
    let translucent_d = textureLoad(g_t_depth, pixel, 0);

    let ndc_far = vec3<f32>(in.uv.x * 2.0 - 1.0, 1.0 - in.uv.y * 2.0, 0.0);
    let world_h = frame.inv_view_proj * vec4<f32>(ndc_far, 1.0);
    let view_dir = normalize((world_h.xyz / world_h.w) - frame.camera_pos.xyz);

    // Background HDR colour: opaque shaded, or sky if no opaque.
    var background = get_sky_color(view_dir);
    if (opaque_d > 0.0) {
        background = shade_opaque_advanced(pixel, in.clip_position.xy, false);
    }

    // Translucent layer in front. Two paths:
    //
    // * **Water / ice** — energy-conserving Fresnel split between
    //   reflection and refraction. Both rays raymarch the opaque
    //   layer in screen space (`ssr()` with the appropriate
    //   reflected / refracted direction); reflection misses the sky,
    //   refraction misses fall back to the original background. The
    //   final colour is `R · reflection + (1 − R) · refraction`,
    //   with `R` from Schlick's approximation (and `R = 1` when the
    //   refract direction would total-internally-reflect).
    // * **Other translucents** (leaves, glass) — straight alpha-mix
    //   of the lit colour over the background. No refraction.
    //
    // The C++ `bg_with_water = mix(bg, water_lit, 0.02)` heuristic
    // and the chunk-side water-α-0.02 hack are both gone. The
    // surface contribution now comes from a real refraction lookup,
    // optionally tinted by the water albedo (heuristic Beer's law).
    var color = background;
    if (translucent_d > 0.0) {
        let mat = textureLoad(g_t_material, pixel, 0).r;
        let water_layer = frame.material_layers.x;
        let ice_layer = frame.material_layers.y;
        let is_water = mat == water_layer;
        let is_ice = mat == ice_layer;
        if (is_water || is_ice) {
            let view_relative = reconstruct_view_relative(in.uv, translucent_d);
            let view_to_surface = normalize(view_relative);
            var t_normal = oct_decode(textureLoad(g_t_normal, pixel, 0).rg);
            // `inside = surface normal points away from camera`.
            let inside = dot(view_relative, t_normal) > 0.0;

            // Water-wave normal perturbation — only on top-of-water-
            // column surfaces (`normal.y > 0.9`) and only when the
            // perturbed normal doesn't flip the surface's relative
            // orientation. World-space sample uses the
            // `player_coord_mod + frac` trick so the wave coords
            // don't lose precision far from the world origin.
            if (is_water && t_normal.y > 0.9) {
                let surface_world = view_relative + vec3<f32>(frame.player_coord_mod.xyz) + frame.player_coord_frac.xyz;
                let wave_n = calc_wave_normal(surface_world);
                var cos_check = dot(- view_to_surface, wave_n);
                if (inside) {
                    cos_check = - cos_check;
                }
                if (cos_check >= 0.0) {
                    t_normal = wave_n;
                }
            }

            // Geometric normal facing the camera — needed for
            // `refract()` (WGSL expects N to point against the
            // incident ray) and for a clean `cos_theta`.
            let n_geom = select(t_normal, - t_normal, inside);

            // IORs in the order the ray crosses them. eta = n_from /
            // n_to. Outside view: air → water (η < 1, ray bends
            // toward normal). Underwater view: water → air (η > 1,
            // ray bends away — TIR at grazing angles).
            let ior = select(1.31, 1.33, is_water);
            let n_from = select(1.0, ior, inside);
            let n_to = select(ior, 1.0, inside);
            let eta = n_from / n_to;

            let reflect_dir = reflect(view_to_surface, n_geom);
            let refract_dir = refract(view_to_surface, n_geom, eta);
            // WGSL `refract` returns the zero vector on total
            // internal reflection.
            let tir = dot(refract_dir, refract_dir) < 0.0001;

            let cos_theta = max(0.0, dot(- view_to_surface, n_geom));
            var R = schlick(n_from, n_to, cos_theta);
            if (tir) {
                R = 1.0;
            }
            let T = 1.0 - R;

            // Common ray origin in clip space for both raymarches.
            let chunk_ndc = vec3<f32>(in.uv.x * 2.0 - 1.0, 1.0 - in.uv.y * 2.0, translucent_d);
            let org_clip = vec4<f32>(chunk_ndc, 1.0);

            // ---- Reflection ----
            // Base colour: sky outside, dim grey underwater (no sky
            // visible through a downward reflect ray). SSR overlays
            // the opaque hit when it lands.
            var reflection: vec3<f32>;
            if (inside) {
                reflection = vec3<f32>(0.1);
            }
            else {
                reflection = get_sky_color(reflect_dir);
            }
            let reflect_dir_clip = frame.view_proj * vec4<f32>(reflect_dir, 0.0);
            // Underwater reflection rays bounce off the surface back
            // into the underwater scene, so they should: (a) skip
            // direct sun at the hit (the water column has absorbed
            // it), and (b) accept any hit without normal-rejection
            // — same reasoning as the refraction path, foreground
            // occluders along the bent ray would otherwise punch
            // mirror-holes into the underwater reflection.
            let ssr_reflect = ssr(org_clip, reflect_dir_clip, in.clip_position.xy, inside, !inside);
            reflection = mix(reflection, ssr_reflect.rgb, ssr_reflect.a);

            // ---- Refraction ----
            // TIR → no refraction (T = 0 anyway, value doesn't
            // matter). Underwater → ray exits into air, sample sky
            // directly (raymarching air for opaque is pointless;
            // sky lookup is always valid → confidence = 1).
            // Above water → SSR-refract: the bent ray hits opaque
            // geometry beneath the surface. Hit confidence is the
            // SSR alpha (with edge fade); on miss, the refraction's
            // energy weight is donated to the reflection branch,
            // which has its own coherent sky fallback. The result is
            // a smooth fade from "refractive water" toward "mirror
            // water" wherever SSR-refract can't see the bottom —
            // physically defensible and avoids the jarring boundary
            // a constant deep-water fallback would produce. Hits
            // get a subtle Beer's-law tint by water albedo.
            var refraction: vec3<f32>;
            var refraction_confidence: f32 = 1.0;
            if (tir) {
                refraction = vec3<f32>(0.0);
                refraction_confidence = 0.0;
            }
            else if (inside) {
                refraction = get_sky_color(refract_dir);
            }
            else {
                let refract_dir_clip = frame.view_proj * vec4<f32>(refract_dir, 0.0);
                let ssr_refract = ssr(org_clip, refract_dir_clip, in.clip_position.xy, true, false);
                let water_albedo = textureLoad(g_t_diffuse, pixel, 0).rgb;
                let tint = mix(vec3<f32>(1.0), water_albedo, 0.3);
                refraction = ssr_refract.rgb * tint;
                refraction_confidence = ssr_refract.a;
            }

            // Energy-conserving combine. The refraction term's
            // weight is scaled by hit confidence; the missing
            // weight cascades to reflection, which is itself a
            // cascade (SSR-reflect → sky). Net: every pixel
            // produces a coherent colour with no hard boundaries
            // even when SSR-refract finds nothing.
            //
            //   effective_T = T · confidence
            //   effective_R = 1 - effective_T  (= R + T·(1-confidence))
            //   color       = effective_R · reflection + effective_T · refraction
            let effective_T = T * refraction_confidence;
            let effective_R = 1.0 - effective_T;
            color = effective_R * reflection + effective_T * refraction;
        }
        else {
            let lit = shade_translucent_advanced(pixel, in.clip_position.xy);
            color = mix(background, lit.rgb, lit.a);
        }
    }

    // Volumetric clouds — layer on top of `color`, but cap the
    // raymarch at the closest chunk surface so opaque / translucent
    // geometry occludes clouds beyond it. With reversed-Z, the
    // closer surface has the larger depth value; we take `max` of
    // the two layer depths and unproject to a world-space distance.
    // When neither layer drew (sky pixel) the cloud raymarch goes
    // its full extent (limited by `render_distance` inside `cloud`).
    if (volumetric_clouds) {
        var max_dist = 65536.0;
        let closest_depth = max(opaque_d, translucent_d);
        if (closest_depth > 0.0) {
            max_dist = length(reconstruct_view_relative(in.uv, closest_depth));
        }
        let cloud_result = cloud(frame.camera_pos.xyz, view_dir, max_dist, in.clip_position.xy);
        color = mix(color, cloud_result.rgb, cloud_result.a);
    }

    // Anchor the noise binding for the case both `volumetric_clouds`
    // and `ambient_occlusion` are off — wgpu pipeline reflection
    // would otherwise drop the noise binding from the shader and the
    // pipeline-layout entry would be unused.
    color = color + vec3<f32>(anchor_noise_binding(in.uv));
    return vec4<f32>(aces(color * EXPOSURE), 1.0);
}

#version 330 core

in vec2 tex_coord;

layout(location = 0) out vec4 o_frag_color;

uniform sampler2D u_diffuse_buffer;
uniform sampler2D u_normal_buffer;
uniform sampler2D u_material_buffer;
uniform sampler2D u_depth_buffer;
uniform sampler2DShadow u_shadow_texture;
uniform sampler2D u_noise_texture;
uniform float u_buffer_width;
uniform float u_buffer_height;
uniform float u_shadow_texture_resolution;
uniform float u_shadow_fisheye_factor;
uniform float u_shadow_distance;
uniform float u_render_distance;

uniform mat4 u_mvp;
uniform mat4 u_shadow_mvp;
uniform vec3 u_sunlight_dir;
uniform float u_game_time;
uniform int u_repeat_length;
uniform ivec3 u_player_coord_int;
uniform ivec3 u_player_coord_mod;
uniform vec3 u_player_coord_frac;

const float PI = 3.141593;
const float GAMMA = 2.2;
const uint LEAF_ID = 8u, GLASS_ID = 9u, WATER_ID = 10u, LAVA_ID = 11u, GLOWSTONE_ID = 12u, ICE_ID = 15u, IRON_ID = 17u;
const uint INDICATOR_ID = 65535u;
const float NOISE_TEXTURE_SIZE = 256.0;
const vec2 NOISE_TEXTURE_OFFSET = vec2(37.0, 17.0);

// Shadow map
const float SHADOW_UNITS = 32.0;
const float SHADOW_RADIUS = 0.2;
const int SHADOW_SAMPLES = 16;
const float SSAO_RADIUS = 1.0;
const int SSAO_SAMPLES = 16;

// Water reflection
const float WAVE_UNITS = 32.0;
const int WAVE_OCTAVES = 7;
const float WAVE_LEVEL = -0.5;
const float WAVE_SCALE = 0.01;
const float WAVE_MIN_LENGTH = 4.0;
const float WAVE_MAX_LENGTH = 12.0;
const float WAVE_DIRECTION_RANGE = 0.1;
const int REFL_ITERATIONS = 32;
const float REFL_STEP_SCALE = 2.0 / 32.0;

// Volumetric clouds
const float CLOUD_UNITS = 32.0; // 1.0 / 16.0;
const vec3 CLOUD_SCALE = vec3(100.0, 80.0, 100.0);
const float CLOUD_BOTTOM = 100.0, CLOUD_TOP = 65536.0, CLOUD_TRANSITION = 120.0;
const int CLOUD_ITERATIONS = 32;
const float CLOUD_STEP_SCALE = 512.0 / 32.0;

mat4 mvp_inverse;
vec4 fisheye_projection_origin;

float rand(vec2 v) {
    return fract(sin(dot(v, vec2(12.9898, 78.233))) * 43758.5453);
}

uint decode_u16(vec2 v) {
    uint high = uint(v.x * 255.0 + 0.5);
    uint low = uint(v.y * 255.0 + 0.5);
    return high * 256u + low;
}

vec3 divide(vec4 v) {
    return (v / v.w).xyz;
}

// vec4 blend(vec4 fore, vec4 back) {
//     vec3 rgb = fore.rgb * fore.a + back.rgb * (1.0 - fore.a);
//     float alpha = fore.a + back.a * (1.0 - fore.a);
//     return vec4(rgb, alpha);
// }

vec3 blend(vec4 fore, vec3 back) {
    return mix(back, fore.rgb, fore.a);
}

vec4 get_scene_diffuse(vec2 tex_coord) {
    return texture(u_diffuse_buffer, tex_coord);
}

vec3 get_scene_normal(vec2 tex_coord) {
    return normalize(texture(u_normal_buffer, tex_coord).rgb * 2.0 - vec3(1.0));
}

uint get_scene_material(vec2 tex_coord) {
    return decode_u16(texture(u_material_buffer, tex_coord).rg);
}

float get_scene_depth(vec2 tex_coord) {
    return texture(u_depth_buffer, tex_coord).r * 2.0 - 1.0;
}

vec4 tex_coord_to_screen_space_coord(vec2 tex_coord) {
    float depth = get_scene_depth(tex_coord);
    return vec4(tex_coord * 2.0 - vec2(1.0), depth, 1.0);
}

vec2 screen_space_coord_to_tex_coord(vec4 v) {
    vec2 res = divide(v).xy / 2.0 + vec2(0.5);
    return res;
}

float distance_to_edge(vec2 v) {
    return min(min(v.x, 1.0 - v.x), min(v.y, 1.0 - v.y));
}

vec2 fisheye_projection(vec2 position) {
    position -= fisheye_projection_origin.xy;
    float dist = length(position);
    float distort_factor = (1.0 - u_shadow_fisheye_factor) + dist * u_shadow_fisheye_factor;
    return position / distort_factor + fisheye_projection_origin.xy;
}

vec2 fisheye_inverse(vec2 position) {
    position -= fisheye_projection_origin.xy;
    float dist = length(position);
    float distort_factor = (1.0 - dist * u_shadow_fisheye_factor) / (1.0 - u_shadow_fisheye_factor);
    return position / distort_factor + fisheye_projection_origin.xy;
}

// Repeat period = NOISE_TEXTURE_SIZE
float interpolated_noise(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
//    f = smoothstep(0.0, 1.0, f);
    vec2 uv = (p.xz + NOISE_TEXTURE_OFFSET * p.y) + f.xz;
    vec4 v = texture(u_noise_texture, uv / NOISE_TEXTURE_SIZE);
    return mix(v.r, v.b, f.y);
}

// Sample shadow map
float get_shadow_offset(vec3 tex_coord, vec2 offset) {
  return texture(u_shadow_texture, vec3(tex_coord.xy + offset / u_shadow_texture_resolution, tex_coord.z));
}

// Sample shadow map (4 samples)
float get_shadow_quad(vec3 tex_coord, int i) {
    // (i mod 4) controls stratified sampling
    // See: https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-11-shadow-map-antialiasing
    vec2 seed = gl_FragCoord.xy * 0.5 + vec2(float(i) * 0.5, float(i) * 0.25 - 0.125);
    vec2 offset = vec2(greaterThan(fract(seed), vec2(0.5)));
    float res = 0.0;
    res += get_shadow_offset(tex_coord, offset + vec2(-1.5, -1.5));
    res += get_shadow_offset(tex_coord, offset + vec2(+0.5, -1.5));
    res += get_shadow_offset(tex_coord, offset + vec2(+0.5, +0.5));
    res += get_shadow_offset(tex_coord, offset + vec2(-1.5, +0.5));
    return res * 0.25;
}

float calc_sunlight_inner(vec3 coord, bool quad, int i) {
    // Shadow map coordinates
    vec3 shadow_coord = divide(u_shadow_mvp * vec4(coord, 1.0));
    shadow_coord = vec3(fisheye_projection(shadow_coord.xy), shadow_coord.z);
    if (shadow_coord.x < -1.0 || shadow_coord.x > 1.0 || shadow_coord.y < -1.0 || shadow_coord.y > 1.0) return 1.0;
    vec3 tex_coord = shadow_coord * 0.5 + vec3(0.5);

    // Sample shadow map
    float res = quad ? get_shadow_quad(tex_coord, i) : get_shadow_offset(tex_coord, vec2(0.0));
    float dist = length(divide(vec4(coord, 1.0)));
    float factor = smoothstep(0.8, 1.0, dist / u_shadow_distance);
    return mix(res, 1.0, factor);
}

float calc_sunlight(vec3 coord, vec3 normal) {
    float cos_theta = dot(normal, -u_sunlight_dir);
    if (cos_theta <= 0.0) return 0.0;
    coord -= u_sunlight_dir * 0.1;

    // Light space coordinates
    vec3 shadow_coord = divide(u_shadow_mvp * vec4(coord, 1.0));
    shadow_coord = vec3(fisheye_projection(shadow_coord.xy), shadow_coord.z);

    // Neighbor pixel distance in light space
    float shift = 2.0 / u_shadow_texture_resolution;
    shift = length(fisheye_inverse(shadow_coord.xy + shift * normalize(shadow_coord.xy)) - fisheye_inverse(shadow_coord.xy));

    // Approximate world space distance for moving one pixel in shadow map
    float normal_bias = shift * u_shadow_distance;

#ifdef SOFT_SHADOW
    // Increased normal bias due to offset sampling (max 1.5 + 1 pixels)
    normal_bias *= 2.5;
#endif

#ifdef MERGE_FACE
    // Additive geometric bias due to non-linear transformation (magic number)
    normal_bias += (16.0 / u_shadow_distance) * u_shadow_fisheye_factor * 4.0;
#endif

#ifdef SOFT_SHADOW
    vec3 tangent = normalize(cross(normal, vec3(1.0, 1.0, 1.0)));
    vec3 bitangent = cross(normal, tangent);

    // Each `calc_sunlight_inner` call takes 4 samples
    float res = 0.0;
    float count = 0.0;
    for (int i = 0; i * 4 < SHADOW_SAMPLES; i++) {
        float ratio = float(i * 4) / float(SHADOW_SAMPLES);
        vec3 offset = vec3(
            rand(gl_FragCoord.xy + vec2(ratio, 0.0)) * 2.0 - 1.0,
            rand(gl_FragCoord.xy + vec2(0.0, ratio)) * 2.0 - 1.0,
            normal_bias
        );
        offset.xy *= SHADOW_RADIUS;
        vec3 sample_coord = coord + mat3(tangent, bitangent, normal) * offset;
        res += calc_sunlight_inner(sample_coord, true, i);
        count += 1.0;
    }
    return res / count * cos_theta;
#else
    vec3 sample_coord = coord + normal * normal_bias;
    return calc_sunlight_inner(sample_coord, false, 0) * cos_theta;
#endif
}

float calc_ambient(vec3 coord, vec3 normal) {
#ifdef AMBIENT_OCCLUSION
    vec3 tangent = normalize(cross(normal, vec3(1.0, 1.0, 1.0)));
    vec3 bitangent = cross(normal, tangent);

    float res = 0.0;
    float count = 0.0;
    for (int i = 0; i < SSAO_SAMPLES; i++) {
        float ratio = float(i) / float(SSAO_SAMPLES);
        vec3 offset = vec3(
            rand(gl_FragCoord.xy + vec2(ratio, 0.0)) * 2.0 - 1.0,
            rand(gl_FragCoord.xy + vec2(0.0, ratio)) * 2.0 - 1.0,
            rand(gl_FragCoord.xy + vec2(ratio, ratio))
        );
        offset *= SSAO_RADIUS;

        vec3 sample_coord = coord + mat3(tangent, bitangent, normal) * offset;
        vec4 screen_space_sample_coord = u_mvp * vec4(sample_coord, 1.0);
        vec2 tex_space_sample_coord = screen_space_coord_to_tex_coord(screen_space_sample_coord);
        if (get_scene_depth(tex_space_sample_coord) < divide(screen_space_sample_coord).z) {
            res += smoothstep(0.8, 1.0, 1.0 - distance_to_edge(tex_space_sample_coord) * 2.0);
        } else {
            res += 1.0;
        }
        count += 1.0;
    }
    return res / count;
#else
    return 1.0;
#endif
}

vec3 get_sky_color(vec3 dir) {
    dir = normalize(dir);
    vec3 tangent = normalize(cross(vec3(0.0, 1.0, 0.0), -u_sunlight_dir));
    vec3 bitangent = cross(tangent, -u_sunlight_dir);
    vec3 local = vec3(dot(dir, tangent), dot(dir, bitangent), dot(dir, -u_sunlight_dir));
    if (abs(local.x) < 0.03 && abs(local.y) < 0.03 && local.z > 0.0) {
        return vec3(3.5, 3.0, 2.9) * 10.0;
    }
    return mix(
        vec3(1.2, 1.6, 2.0),
        vec3(0.3, 0.5, 1.2),
        smoothstep(0.0, 1.0, normalize(dir).y * 2.0)
    ) * 1.0;
}

vec4 diffuse(vec2 tex_coord) {
    uint block_id = get_scene_material(tex_coord);
    if (block_id == 0u) return vec4(0.0);
    vec4 color = get_scene_diffuse(tex_coord);
    if (block_id == INDICATOR_ID) return vec4(color.rgb, 1.0);
    vec3 normal = get_scene_normal(tex_coord);
    vec3 translation = vec3(u_player_coord_mod) + u_player_coord_frac;

    vec4 screen_space_coord = tex_coord_to_screen_space_coord(tex_coord);
    vec4 relative_coord = mvp_inverse * screen_space_coord;
    vec3 world_space_coord = divide(relative_coord) + translation;
    vec3 shadow_coord = floor(world_space_coord * SHADOW_UNITS + normal * 0.5) / SHADOW_UNITS - translation;
    // vec3 shadow_coord = world_space_coord - translation;

    float sunlight = 0.0;
    float ambient = 1.0;
    float glow = 0.0;

    if (block_id != WATER_ID) {
        sunlight = calc_sunlight(shadow_coord, normal);
        ambient = calc_ambient(shadow_coord, normal);
    }

    if (block_id == GLOWSTONE_ID) glow = 5.0;

    return vec4(max(vec3(3.5, 3.0, 2.9) * sunlight + vec3(0.4, 0.5, 0.8) * ambient, glow) * color.rgb, color.a);
}

vec4 diffuse_with_fade(vec2 tex_coord) {
    vec4 color = diffuse(tex_coord);
    if (get_scene_material(tex_coord) != 0u) {
        vec4 screen_space_coord = tex_coord_to_screen_space_coord(tex_coord);
        vec4 relative_coord = mvp_inverse * screen_space_coord;
        float dist = length(divide(relative_coord));
        color.a *= clamp((u_render_distance - dist) / 32.0, 0.0, 1.0);
    }
    return color;
}

vec3 calc_wave_normal(vec3 pos) {
    pos = floor(pos * WAVE_UNITS) / WAVE_UNITS;
    vec3 hs = vec3(0.0);
    for (int i = 0; i < WAVE_OCTAVES; i++) {
        float ratio = float(i) / float(WAVE_OCTAVES);
        // Wave length
        float lambda = WAVE_MIN_LENGTH + rand(vec2(ratio, ratio)) * (WAVE_MAX_LENGTH - WAVE_MIN_LENGTH);
        // Wave number
        float k = 2.0 * PI / lambda;
        // Wave amplitude
        float a = exp(k * WAVE_LEVEL) / k;
        // Phase velocity
        float c = sqrt(9.81 / k);
        // Wave direction
        float angle = 2.0 * PI * ratio * WAVE_DIRECTION_RANGE;
        vec2 direction = vec2(cos(angle), sin(angle));
        // Positions
        vec3 ps = vec3(
            dot(pos.xz + vec2(0.1, 0.0), direction),
            dot(pos.xz, direction),
            dot(pos.xz + vec2(0.0, 0.1), direction)
        );
        // Time
        float t = u_game_time / 30.0;
        // Solve the equation p0 = p - a * sin(k * (p0 + c * t)) by fixed-point iteration
        // Requires that a * k < 1, or that WAVE_LEVEL < 0
        vec3 ps0 = ps;
        ps0 = ps - a * sin(k * (ps0 + c * t));
        ps0 = ps - a * sin(k * (ps0 + c * t));
        ps0 = ps - a * sin(k * (ps0 + c * t));
        hs += -a * cos(k * (ps0 + c * t));
    }
    hs *= WAVE_SCALE;
    vec3 xx = vec3(0.1, hs.x - hs.y, 0.0);
    vec3 zz = vec3(0.0, hs.z - hs.y, 0.1);
    return normalize(cross(zz, xx));
}

float schlick(float n, float m, float cos_theta) {
    if (cos_theta < 0.0) return 1.0;
    float r0 = pow((n - m) / (n + m), 2.0);
    return r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
}

vec4 ssr(vec4 org, vec4 dir, bool inside) {
    vec3 org3 = divide(org);
    vec3 dir3 = normalize(divide(org + dir) - org3);
    dir3 /= length(dir3.xy);

    float step_mult = 1.0;
    vec3 curr3 = org3;
    vec2 best = screen_space_coord_to_tex_coord(vec4(curr3, 1.0));
    bool found = false;
    float found_ratio = 1.0;

    for (int i = 0; i < REFL_ITERATIONS; i++) {
        float ratio = float(i) / float(REFL_ITERATIONS);
        float step = step_mult * REFL_STEP_SCALE * (i == 0 ? 0.5 + rand(gl_FragCoord.xy) : 1.0);

        // Stop if the step size is less than one pixel
        if (step_mult * REFL_STEP_SCALE < 2.0 / max(u_buffer_width, u_buffer_height)) break;

        vec3 next3 = curr3 + dir3 * step;

        // Stop if out of screen
        if (next3.x < -1.0 || next3.x > 1.0 || next3.y < -1.0 || next3.y > 1.0) break;

        vec2 tex_coord = screen_space_coord_to_tex_coord(vec4(next3, 1.0));

        // Check for possible intersection with scene
        float z = get_scene_depth(tex_coord);
        if (z <= next3.z) {
            if (get_scene_material(tex_coord) != 0u) {
                // Filter out some false positives
                vec4 relative_coord = mvp_inverse * vec4(next3.xy, z, 1.0);
                vec4 relative_curr = mvp_inverse * vec4(curr3, 1.0);
                vec3 normal = get_scene_normal(tex_coord);
                if (dot(divide(relative_curr) - divide(relative_coord), normal) >= -0.1) {
                    if (!found) found_ratio = ratio;
                    found = true;
                    best = tex_coord;
                }
            }
            step_mult /= 2.0;
        } else {
            curr3 = next3;
        }
    }

    if (!found) return vec4(0.0);
    vec4 color = diffuse_with_fade(best);
    color.a *= 1.0 - smoothstep(0.8, 1.0, max(1.0 - distance_to_edge(best) * 2.0, found_ratio));
    return color;
}

// Repeat period = NOISE_TEXTURE_SIZE
float cloud_noise(vec3 c) {
    float res = 0.0;
    res += interpolated_noise(c * 1.0) / 1.0;
    res += interpolated_noise(c * 2.0) / 2.0;
    res += interpolated_noise(c * 6.0) / 4.0;
    res += interpolated_noise(c * 24.0) / 12.0;
    return res / (1.0 + 1.0 / 2.0 + 1.0 / 4.0 + 1.0 / 12.0);
}

float calc_cloud_opacity(vec3 pos) {
    pos = floor(pos * CLOUD_UNITS) / CLOUD_UNITS;
    float factor = min(
        smoothstep(CLOUD_BOTTOM, CLOUD_BOTTOM + CLOUD_TRANSITION, pos.y),
        1.0 - smoothstep(CLOUD_TOP - CLOUD_TRANSITION, CLOUD_TOP, pos.y)
    );
    float opacity = clamp(cloud_noise(pos / CLOUD_SCALE) * 2.0 - 1.2, 0.0, 1.0);
    return factor * opacity;
}

vec4 cloud(vec3 org, vec3 dir, float dist, vec3 center, float quality) {
    dir = normalize(dir);
    vec3 curr = org;
    vec3 res = vec3(0.0);
    float remaining = 1.0;
    
    if (curr.y < CLOUD_BOTTOM) {
        if (dir.y <= 0.0) return vec4(0.0);
        curr += dir * (CLOUD_BOTTOM - curr.y) / dir.y;
    } else if (curr.y > CLOUD_TOP) {
        if (dir.y >= 0.0) return vec4(0.0);
        curr += dir * (CLOUD_TOP - curr.y) / dir.y;
    }
    
    for (int i = 0; i < CLOUD_ITERATIONS; i++) {
        float ratio = float(i) / float(CLOUD_ITERATIONS);
        float step = CLOUD_STEP_SCALE / quality * (i == 0 ? 0.5 + rand(gl_FragCoord.xy) : 1.0);
        curr += dir * step;
        
        if (remaining < 0.01) break;
        if (length(curr - org) > dist) break;
        
        if (curr.y >= CLOUD_BOTTOM && curr.y <= CLOUD_TOP) {
            float factor = 1.0;
            factor *= 1.0 - smoothstep(u_render_distance * 0.8, u_render_distance, length(curr - center));
            factor *= 1.0 - smoothstep(dist * 0.8, dist, length(curr - org));
            float transmittence = pow(1.0 - factor * calc_cloud_opacity(curr), step);
            if (transmittence < 0.99) {
                float scattering = 1.0;
                scattering *= pow(1.0 - calc_cloud_opacity(curr - u_sunlight_dir * 8.0), 8.0);
                scattering *= pow(1.0 - calc_cloud_opacity(curr - u_sunlight_dir * 16.0), 8.0);
                // scattering *= pow(1.0 - calc_cloud_opacity(curr - u_sunlight_dir * 32.0), 16.0);
                // scattering *= pow(1.0 - calc_cloud_opacity(curr - u_sunlight_dir * 64.0), 32.0);
                vec3 col = vec3(3.5, 3.0, 2.9) * scattering + vec3(0.35, 0.5, 1.0) * 0.5;
                res += remaining * (1.0 - transmittence) * col;
                remaining *= transmittence;
            }
        } else {
            break;
        }
    }
    return vec4(res, 1.0 - remaining);
}

vec3 aces(vec3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    mvp_inverse = inverse(u_mvp);
    fisheye_projection_origin = u_shadow_mvp * vec4(0.0f, 0.0f, 0.0f, 1.0f);
    fisheye_projection_origin /= fisheye_projection_origin.w;

    vec4 screen_space_coord = tex_coord_to_screen_space_coord(tex_coord);
    vec4 relative_coord = mvp_inverse * screen_space_coord;

    vec3 view_origin = vec3(u_player_coord_mod) + u_player_coord_frac;
    vec3 view_dir = normalize(divide(relative_coord));
    vec3 normal = get_scene_normal(tex_coord);
    vec3 color = get_sky_color(view_dir);
    color = blend(diffuse_with_fade(tex_coord), color);

    uint block_id = get_scene_material(tex_coord);
    if (block_id == WATER_ID || block_id == ICE_ID || block_id == IRON_ID) {
        vec3 reflect_origin = view_origin + divide(relative_coord);
        bool inside = dot(view_origin - reflect_origin, normal) < 0.0;
        
        // Water wave effect
        if (block_id == WATER_ID && normal.y > 0.9) {
            vec3 wave_normal = calc_wave_normal(reflect_origin);

            // Only admit wave if it does not change the relative orientation
            float cos_theta = dot(normalize(view_origin - reflect_origin), wave_normal);
            if (inside) cos_theta = -cos_theta;
            if (cos_theta >= 0.0) normal = wave_normal;
        }

        /*
        // Glossy metal effect
        if (block_id == IRON_ID) {
            vec3 perturb = vec3(
                rand(gl_FragCoord.xy),
                rand(gl_FragCoord.xy + vec2(0.1, 0.0)),
                rand(gl_FragCoord.xy + vec2(0.0, 1.0)));
            perturb = (perturb - vec3(0.5)) * 0.03;
            normal = normalize(normal + perturb);
        }
        */

        // Reflection calculation
        vec3 reflect_dir = reflect(normalize(reflect_origin - view_origin), normal);
        float cos_theta = dot(normalize(view_origin - reflect_origin), normal);
        if (inside) cos_theta = -cos_theta;
        
        vec3 reflection = vec3(0.1);
        if (!inside) {
            reflection = get_sky_color(reflect_dir);
#ifdef VOLUMETRIC_CLOUDS
            reflection = blend(cloud(reflect_origin, reflect_dir, 65536.0, view_origin, 0.5), reflection);
#endif
        }
        vec4 screen_space_reflect_dir = u_mvp * vec4(reflect_dir, 0.0);
        reflection = blend(ssr(screen_space_coord, screen_space_reflect_dir, inside), reflection);

        // Fresnel-Schlick blending
        float albedo = 1.0;
        if (!inside) {
            if (block_id == WATER_ID) albedo *= schlick(1.0, 1.33, cos_theta);
            else if (block_id == ICE_ID) albedo *= schlick(1.0, 2.42, cos_theta);
            else if (block_id == IRON_ID) reflection *= get_scene_diffuse(tex_coord).rgb * vec3(0.5);
        } else {
            // TODO: make this more realistic
            albedo *= smoothstep(0.0, 1.0, 1.0 - cos_theta * cos_theta);
        }

        color = mix(color, reflection, albedo);
    }
    
    // Fog
    float dist = 65536.0;
    if (block_id != 0u) dist = length(divide(relative_coord));
#ifdef VOLUMETRIC_CLOUDS
    color = blend(cloud(view_origin, view_dir, dist, view_origin, 1.0), color);
#endif

    // Component-wise tone mapping
    // color = color / (color + vec3(1.0)); // Reinhard tone mapping
    // color = vec3(1.0) - exp(-color * 0.8); // Exposure tone mapping
    color = aces(color * 0.5);

    // Luminance-based tone mapping
    // float luminance = color.r * 0.2125 + color.g * 0.7154 + color.b * 0.0721;
    // color /= luminance;
    // luminance = luminance / (luminance + 1.0); // Reinhard tone mapping
    // luminance = 1.0 - exp(-luminance * 0.8); // Exposure tone mapping
    // color *= luminance;
    
    // Gamma encoding
    color = pow(color, vec3(1.0 / GAMMA));
    o_frag_color = vec4(color, 1.0);
    gl_FragDepth = divide(screen_space_coord).z * 0.5 + 0.5;
}

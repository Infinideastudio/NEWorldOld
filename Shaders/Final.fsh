#version 330 core

in vec2 tex_coord;

layout(location = 0) out vec4 o_frag_color;

uniform sampler2D u_diffuse_buffer;
uniform sampler2D u_normal_buffer;
uniform sampler2D u_material_buffer;
uniform sampler2D u_depth_buffer;
uniform sampler2DShadow u_shadow_texture;
uniform sampler2D u_noise_texture;

uniform float u_buffer_view_width;
uniform float u_buffer_view_height;
uniform float u_buffer_size;
uniform mat4 u_proj;
uniform mat4 u_modl;
uniform mat4 u_proj_inv;
uniform mat4 u_modl_inv;

uniform float u_render_distance;
uniform float u_shadow_distance;
uniform mat4 u_shadow_proj;
uniform mat4 u_shadow_modl;
uniform float u_shadow_texture_resolution;
uniform float u_shadow_fisheye_factor;

uniform vec3 u_sunlight_dir;
uniform float u_game_time;
uniform ivec3 u_player_coord_int;
uniform vec3 u_player_coord_frac;

const float PI = 3.141593;
const float GAMMA = 2.2;
const int LEAF_ID = 8, GLASS_ID = 9, WATER_ID = 10, LAVA_ID = 11, GLOWSTONE_ID = 12, ICE_ID = 15, IRON_ID = 17;
const int REPEAT_LENGTH = 1024;
const vec2 NOISE_TEXTURE_OFFSET = vec2(37.0, 17.0);

// Shadow map
const mat4 SHADOW_NORM = mat4(
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 0.5, 0.0,
	0.5, 0.5, 0.5, 1.0
);
const float SHADOW_UNITS = 32.0;
const float SHADOW_SMOOTH = 0.1;
const int SHADOW_SAMPLES = 16;

// Water reflection
const float WAVE_UNITS = 32.0;
const vec2 WAVE_SCALE = vec2(2.0, 2.0);
const int WAVE_ITERATIONS = 32;
const float WAVE_STEP_SCALE = 2.0 / 28.0;

// Volumetric clouds
const float CLOUD_UNITS = 32.0; // 1.0 / 16.0;
const vec3 CLOUD_SCALE = vec3(100.0, 80.0, 100.0);
const float CLOUD_BOTTOM = 100.0, CLOUD_TOP = 65536.0, CLOUD_TRANSITION = 120.0;
const int CLOUD_ITERATIONS = 32;
const float CLOUD_STEP_SCALE = 16.0;

float buffer_view_width_fraction;
float buffer_view_height_fraction;
ivec3 player_coord_mod;

float rand(vec2 v) {
	return fract(sin(dot(v, vec2(12.9898, 78.233))) * 43758.5453);
}

int mod_int(int v, int m) {
	int res = v - v / m * m;
	if (res < 0) return res + m;
	return res;
}

int decode_u16(vec2 v) {
	int high = int(v.x * 255.0 + 0.5);
	int low = int(v.y * 255.0 + 0.5);
	return high * 256 + low;
}

vec3 divide(vec4 v) {
	return (v / v.w).xyz;
}

vec3 screen_space_to_texture_coord(vec4 v) {
	vec3 res = divide(v) / 2.0 + vec3(0.5);
	res.x *= buffer_view_width_fraction;
	res.y *= buffer_view_height_fraction;
	return res;
}

float get_scene_depth(vec2 tex_coord) {
	return texture(u_depth_buffer, tex_coord).r * 2.0 - 1.0;
}

vec3 get_scene_coord(vec2 tex_coord) {
	float depth = get_scene_depth(tex_coord);
	tex_coord = vec2(tex_coord.x / buffer_view_width_fraction, tex_coord.y / buffer_view_height_fraction) * 2.0 - vec2(1.0);
	vec4 res = u_proj_inv * vec4(tex_coord, depth, 1.0);
	return res.xyz / res.w;
}

vec4 get_scene_diffuse(vec2 tex_coord) {
	return texture(u_diffuse_buffer, tex_coord);
}

vec3 get_scene_normal(vec2 tex_coord) {
	return normalize(texture(u_normal_buffer, tex_coord).rgb * 2.0 - vec3(1.0));
}

int get_scene_material(vec2 tex_coord) {
	return decode_u16(texture(u_material_buffer, tex_coord).rg);
}

float distance_to_edge(vec2 v) {
	v.x *= u_buffer_size / u_buffer_view_width;
	v.y *= u_buffer_size / u_buffer_view_height;
	return min(min(v.x, 1.0 - v.x), min(v.y, 1.0 - v.y));
}

vec2 fisheye_projection(vec2 position) {
	float dist = length(position);
	float distort_factor = (1.0 - u_shadow_fisheye_factor) + dist * u_shadow_fisheye_factor;
	position /= distort_factor;
	return position;
}

vec2 fisheye_inverse(vec2 position) {
	float dist = length(position);
	float distort_factor = (1.0 - dist * u_shadow_fisheye_factor) / (1.0 - u_shadow_fisheye_factor);
	position /= distort_factor;
	return position;
}

float interpolated_noise(vec3 x) {
	vec3 p = floor(x);
	vec3 f = fract(x);
//	f = smoothstep(0.0, 1.0, f);
	vec2 uv = (p.xy + NOISE_TEXTURE_OFFSET * p.z) + f.xy;
	vec2 v = texture(u_noise_texture, uv / 256.0).rb;
	return mix(v.x, v.y, f.z);
}

float sample_shadow(vec4 coord, float bias) {
	coord.z -= bias;
	coord = SHADOW_NORM * coord;
	return texture(u_shadow_texture, divide(coord));
}

float shadow(vec3 coord, float slope /* vec3 normal */) {
	// Shadow calculation
	vec4 shadow_coord = vec4(coord, 1.0);
	shadow_coord = u_shadow_proj * u_shadow_modl * shadow_coord;
	shadow_coord = vec4(fisheye_projection(shadow_coord.xy), shadow_coord.zw);

	if (shadow_coord.x >= -1.0 && shadow_coord.x < 1.0 && shadow_coord.y >= -1.0 && shadow_coord.y < 1.0 && shadow_coord.z < 1.0) {
		float dist = length(u_modl * vec4(coord, 1.0));
		float shift = 2.0 / u_shadow_texture_resolution;

		// Neighbor pixel distance in light space
		shift = length(fisheye_inverse(shadow_coord.xy + shift * normalize(shadow_coord.xy)) - fisheye_inverse(shadow_coord.xy));

		// Combined bias
		float bias = slope * shift * 0.4;
		float factor = 1.0 - smoothstep(0.8, 1.0, dist / u_shadow_distance);
		return mix(1.0, sample_shadow(shadow_coord, bias), factor);
	}
	return 1.0;
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
		vec3(0.25, 0.4, 1.0),
		smoothstep(0.0, 1.0, normalize(dir).y * 2.0)
	) * 1.0;
}

vec4 diffuse(vec2 tex_coord) {
	int block_id_i = get_scene_material(tex_coord);
	if (block_id_i == 0) return vec4(0.0);
	vec4 color = get_scene_diffuse(tex_coord);
	vec3 normal = get_scene_normal(tex_coord);
	vec3 tangent = normalize(cross(normal, vec3(1.0, 1.0, 1.0)));
	vec3 bitangent = cross(normal, tangent);
	vec3 translation = vec3(player_coord_mod) + u_player_coord_frac;

	vec3 camera_space_coord = get_scene_coord(tex_coord);
	vec4 screen_space_coord = u_proj * vec4(camera_space_coord, 1.0);
	vec3 world_space_coord = divide(u_modl_inv * vec4(camera_space_coord, 1.0)) + translation;
	vec3 shadow_coord = floor(world_space_coord * SHADOW_UNITS + normal * 0.5) / SHADOW_UNITS - translation;

	float sunlight = 0.0;
	float cos_theta = dot(normal, -u_sunlight_dir);
	float slope = sqrt(1.0 - cos_theta * cos_theta) / cos_theta;

	/*
	// Simulate subsurface scattering for certain blocks
	if (cos_theta <= 0.0) {
		shadow_coord -= u_sunlight_dir * 0.3;
		slope = 0.0;
	}
	*/

	// Sample shadow map
	if (block_id_i != WATER_ID && cos_theta > 0.0) {
		for (int i = 0; i < SHADOW_SAMPLES; i++) {
			float ratio = float(i) / float(SHADOW_SAMPLES);
			vec3 offset = (
				rand(gl_FragCoord.xy + vec2(ratio, ratio)) * normal +
				(2.0 * rand(gl_FragCoord.xy + vec2(ratio, 0.0)) - vec3(1.0)) * tangent +
				(2.0 * rand(gl_FragCoord.xy + vec2(0.0, ratio)) - vec3(1.0)) * bitangent) * SHADOW_SMOOTH;
			sunlight += shadow(shadow_coord + offset, slope);
		}
		sunlight /= float(SHADOW_SAMPLES);
		sunlight *= cos_theta;
	}

	float glow = 0.0;
	if (block_id_i == GLOWSTONE_ID) glow = 5.0;

	return vec4(max(vec3(3.5, 3.0, 2.9) * sunlight + vec3(0.35, 0.56, 0.96) * 0.5, glow) * color.rgb, color.a);
}

vec3 diffuse_background(vec2 tex_coord) {
	vec3 view_dir = normalize(divide(u_modl_inv * u_proj_inv * vec4(tex_coord / vec2(buffer_view_width_fraction, buffer_view_height_fraction) * 2.0 - vec2(1.0), -1.0, 1.0)));
	vec3 sky = get_sky_color(view_dir);
	vec4 color = diffuse(tex_coord);
	// Fog
	if (get_scene_material(tex_coord) != 0) {
		vec3 camera_space_coord = get_scene_coord(tex_coord);
		float dist = length(camera_space_coord);
		color.a *= clamp((u_render_distance - dist) / 32.0, 0.0, 1.0);
	}
	return mix(sky, color.rgb, color.a);
}

float wave_noise(vec3 c) {
	float res = 0.0;
	res += interpolated_noise(c * 1.0) / 1.0;
	res += interpolated_noise(c * 2.0) / 2.0;
	res += interpolated_noise(c * vec3(6.0, 6.0, 0.0)) / 4.0;
	res += interpolated_noise(c * vec3(24.0, 24.0, 0.0)) / 12.0;
	return res / (1.0 + 1.0 / 2.0 + 1.0 / 4.0 + 1.0 / 12.0);
}

float calc_wave_height(vec2 v, float factor) {
	float t = u_game_time;
	return wave_noise(vec3((v + vec2(0.01 * t, 0.007 * t)) / WAVE_SCALE, 0.001 * t)) * 0.1 * factor;
}

vec3 calc_wave_normal(vec3 v, float factor) {
	vec2 d = vec2(0.02) * WAVE_SCALE;
	float hv = calc_wave_height(v.xz, factor);
	vec3 xx = vec3(d.x, calc_wave_height(v.xz + vec2(d.x, 0.0), factor) - hv, 0.0);
	vec3 zz = vec3(0.0, calc_wave_height(v.xz + vec2(0.0, d.y), factor) - hv, d.y);
	return normalize(cross(zz, xx));
}

float schlick(float n, float m, float cos_theta) {
	if (cos_theta < 0.0) return 1.0;
	float r0 = pow((n - m) / (n + m), 2.0);
	return r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);
}

vec3 ssr(vec4 org, vec4 dir, vec3 base_color, bool inside) {
	vec3 org3 = divide(org);
	vec3 dir3 = normalize(divide(org + dir) - org3);
	dir3 /= length(dir3.xy);

	vec3 curr3 = org3;
	float curr_dist = length(divide(u_proj_inv * vec4(curr3, 1.0)));
	float step = 1.0;
	bool found = false;
	vec2 best = screen_space_to_texture_coord(vec4(curr3, 1.0)).xy;
	int self = get_scene_material(best);
	float ratio = 1.0;

	for (int i = 0; i < WAVE_ITERATIONS; i++) {
		vec3 next3 = curr3 + dir3 * step * WAVE_STEP_SCALE * (i == 0 ? (1.0 + rand(gl_FragCoord.xy)) : 1.0);
		if (next3.x >= -1.0 && next3.x < 1.0 && next3.y >= -1.0 && next3.y < 1.0 && next3.z >= -1.0 && next3.z < 1.0) {
			vec2 tex_coord = screen_space_to_texture_coord(vec4(next3, 1.0)).xy;
			float next_dist = next3.z;
			float pixel_dist = get_scene_depth(tex_coord);
			if (get_scene_material(tex_coord) != 0 && pixel_dist < next_dist && next_dist - pixel_dist < abs(next_dist - curr_dist) * 3.0) {
				if (!found) ratio = float(i) / float(WAVE_ITERATIONS);
				found = true;
				best = tex_coord;
				step /= 2.0;
			} else {
				curr3 = next3;
				curr_dist = next_dist;
			}
		} else {
			step /= 2.0;
		}
	}

	if (!found) return base_color;
	float factor = smoothstep(0.8, 1.0, max(1.0 - distance_to_edge(best) * 2.0, ratio));
	return mix(diffuse_background(best), base_color, factor);
}

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

vec3 cloud(vec3 org, vec3 dir, vec3 base_color, float dist, vec3 center, float quality) {
	vec3 curr = org;
	dir = normalize(dir);
	vec3 res = vec3(0.0);
	float acc = 1.0;
	
	if (curr.y < CLOUD_BOTTOM) {
		if (dir.y <= 0.0) return base_color;
		curr += dir * (CLOUD_BOTTOM - curr.y) / dir.y;
	} else if (curr.y > CLOUD_TOP) {
		if (dir.y >= 0.0) return base_color;
		curr += dir * (CLOUD_TOP - curr.y) / dir.y;
	}
	
	for (int i = 0; i < CLOUD_ITERATIONS; i++) {
		float step = CLOUD_STEP_SCALE / quality * (i == 0 ? rand(gl_FragCoord.xy) : 1.0);
		curr += dir * step;
		
		if (acc < 0.01) break;
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
				vec3 col = vec3(3.5, 3.0, 2.9) * scattering + vec3(0.25, 0.4, 1.0) * 0.5;
				res += acc * (1.0 - transmittence) * col;
				acc *= transmittence;
			}
		} else {
			break;
		}
	}
	
	return res + acc * base_color;
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
	buffer_view_width_fraction = u_buffer_view_width / u_buffer_size;
	buffer_view_height_fraction = u_buffer_view_height / u_buffer_size;
	player_coord_mod = ivec3(
		mod_int(u_player_coord_int.x, REPEAT_LENGTH * int(CLOUD_SCALE.x + 0.5)),
		mod_int(u_player_coord_int.y, REPEAT_LENGTH * int(CLOUD_SCALE.y + 0.5)),
		mod_int(u_player_coord_int.z, REPEAT_LENGTH * int(CLOUD_SCALE.z + 0.5))
	);
	vec2 tex_coord = tex_coord * vec2(buffer_view_width_fraction, buffer_view_height_fraction);
	int block_id_i = get_scene_material(tex_coord);
	
	vec3 camera_space_coord = get_scene_coord(tex_coord);
	vec4 screen_space_coord = u_proj * vec4(camera_space_coord, 1.0);

	vec3 view_origin = vec3(player_coord_mod) + u_player_coord_frac;
	vec3 view_dir = normalize(divide(u_modl_inv * u_proj_inv * vec4(tex_coord / vec2(buffer_view_width_fraction, buffer_view_height_fraction) * 2.0 - vec2(1.0), -1.0, 1.0)));
	vec3 color = diffuse_background(tex_coord);
	vec3 normal = get_scene_normal(tex_coord);
	
	if (block_id_i == WATER_ID || block_id_i == ICE_ID || block_id_i == IRON_ID) {
		vec3 reflect_origin = view_origin + divide(u_modl_inv * vec4(camera_space_coord, 1.0));
		bool inside = dot(view_origin - reflect_origin, normal) < 0.0;
		
		// Water wave effect
		if (block_id_i == WATER_ID && normal.y > 0.9) {
			vec3 wave_coord = floor(reflect_origin * WAVE_UNITS) / WAVE_UNITS;
			vec3 wave_normal = calc_wave_normal(wave_coord, 1.0 - smoothstep(0.0, 0.5, length(camera_space_coord) / u_render_distance));

			// Only admit wave if it does not change the relative orientation
			float cos_theta = dot(normalize(view_origin - reflect_origin), wave_normal);
			if (inside) cos_theta = -cos_theta;
			if (cos_theta >= 0.0) normal = wave_normal;
		}

		// Glossy metal effect
		if (block_id_i == IRON_ID) {
			vec3 perturb = vec3(
				rand(gl_FragCoord.xy),
				rand(gl_FragCoord.xy + vec2(0.1, 0.0)),
				rand(gl_FragCoord.xy + vec2(0.0, 1.0)));
			perturb = (perturb - vec3(0.5)) * 0.03;
			normal = normalize(normal + perturb);
		}

		// Reflection calculation
		vec3 reflect_dir = reflect(normalize(reflect_origin - view_origin), normal);
		float cos_theta = dot(normalize(view_origin - reflect_origin), normal);
		if (inside) cos_theta = -cos_theta;
		
		vec3 sky = get_sky_color(reflect_dir);
		vec3 reflection;
		if (!inside) {
			reflection = sky;
#ifdef VOLUMETRIC_CLOUDS
			reflection = cloud(reflect_origin, reflect_dir, sky, 65536.0, view_origin, 0.5);
#endif
		} else {
			reflection = vec3(0.1);
		}
		vec4 screen_space_reflect_dir = u_proj * u_modl * vec4(reflect_dir, 0.0);
		reflection = ssr(screen_space_coord, screen_space_reflect_dir, reflection, inside);

		// Fresnel-Schlick blending
		float albedo = 1.0;
		if (!inside) {
			if (block_id_i == WATER_ID) albedo *= schlick(1.0, 1.33, cos_theta);
			else if (block_id_i == ICE_ID) albedo *= schlick(1.0, 2.42, cos_theta);
			else if (block_id_i == IRON_ID) reflection *= color;
		} else {
			// TODO: make this more realistic
			albedo *= smoothstep(0.0, 1.0, 1.0 - cos_theta * cos_theta);
		}
		color = mix(color, reflection, albedo);
	}
	
	// Fog
	float dist = 65536.0;
	if (block_id_i != 0) {
		dist = length(camera_space_coord);
	}
#ifdef VOLUMETRIC_CLOUDS
	color = cloud(view_origin, view_dir, color, dist, view_origin, 1.0);
#endif

	// Component-wise tone mapping
	// color = color / (color + vec3(1.0)); // Reinhard tone mapping
	// color = vec3(1.0) - exp(-color * 0.8); // Exposure tone mapping
	color = aces(color * 0.8);

	// Luminance-based tone mapping
	// float luminance = (color.r + color.g + color.b) / 3.0;
	// color /= luminance;
	// luminance = luminance / (luminance + 1.0); // Reinhard tone mapping
	// luminance = 1.0 - exp(-luminance * 0.8); // Exposure tone mapping
	// color *= luminance;
	
	// GAMMA encoding
	color = pow(color, vec3(1.0 / GAMMA));
	o_frag_color = vec4(color, 1.0);
	gl_FragDepth = divide(screen_space_coord).z * 0.5 + 0.5;
}

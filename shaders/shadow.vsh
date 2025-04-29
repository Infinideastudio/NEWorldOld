#version 330 core

layout(location = 0) in vec3 a_coord;
layout(location = 1) in vec3 a_tex_coord;
layout(location = 2) in float a_color;
layout(location = 3) in vec3 a_normal;
layout(location = 4) in float a_block_id;

out vec3 coord;
out vec3 tex_coord;

uniform mat4 u_mvp;
uniform vec3 u_translation;
uniform float u_game_time;
uniform float u_shadow_fisheye_factor;

const float PI = 3.1415926f;
const int LEAF_ID = 8;

vec4 fisheye_projection_origin;

vec2 fisheye_projection(vec2 position) {
	position -= fisheye_projection_origin.xy;
	float dist = length(position);
	float distort_factor = (1.0 - u_shadow_fisheye_factor) + dist * u_shadow_fisheye_factor;
	return position / distort_factor + fisheye_projection_origin.xy;
}

void main() {
	coord = a_coord;
	int block_id_i = int(a_block_id + 0.5f);
	if (block_id_i == LEAF_ID) {
		float a = u_game_time * 0.2f;
		coord += vec3(sin(coord.x + a), sin(coord.y * 10.0f + a + PI / 3.0f * 2.0f), sin(coord.z * 10.0f + a + PI / 3.0f * 4.0f)) * 0.005f;
	}
	tex_coord = a_tex_coord;

	fisheye_projection_origin = u_mvp * vec4(0.0f, 0.0f, 0.0f, 1.0f);
	fisheye_projection_origin /= fisheye_projection_origin.w;
	gl_Position = u_mvp * vec4(coord + u_translation, 1.0f);
	gl_Position /= gl_Position.w;
	gl_Position = vec4(fisheye_projection(gl_Position.xy), gl_Position.zw);
}

#version 330 core

layout(location = 0) in vec3 a_coord;
layout(location = 1) in vec3 a_tex_coord;
layout(location = 2) in float a_color;
layout(location = 3) in vec3 a_normal;
layout(location = 4) in float a_block_id;

out vec3 coord;
out vec3 tex_coord;
out vec3 color;
out vec3 normal;
out float block_id;

uniform mat4 u_proj;
uniform mat4 u_modl;
uniform mat4 u_translation;
uniform float u_game_time;

const float PI = 3.1415926f;
const int LEAF_ID = 8;

void main() {
	coord = a_coord;
	int block_id_i = int(a_block_id + 0.5f);
	if (block_id_i == LEAF_ID) {
		float a = u_game_time * 0.2f;
		coord += vec3(sin(coord.x + a), sin(coord.y * 10.0f + a + PI / 3.0f * 2.0f), sin(coord.z * 10.0f + a + PI / 3.0f * 4.0f)) * 0.005f;
	}
	tex_coord = a_tex_coord;
	color = vec3(a_color);
	normal = a_normal;
	block_id = a_block_id;

	gl_Position = u_proj * u_modl * u_translation * vec4(coord, 1.0f);
}

#version 330 core

layout(location = 0) in vec3 a_coord;
layout(location = 1) in vec3 a_tex_coord;
layout(location = 2) in vec3 a_color;
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

void main() {
	coord = a_coord;
	tex_coord = a_tex_coord;
	color = a_color;
	normal = a_normal;
	block_id = a_block_id;

	gl_Position = u_proj * u_modl * u_translation * vec4(coord, 1.0f);
}

#version 330 core

layout(location = 0) in vec3 a_coord;
layout(location = 1) in vec3 a_tex_coord;
layout(location = 2) in float a_color;

out vec3 coord;
out vec3 tex_coord;
out vec3 color;

uniform mat4 u_proj;
uniform mat4 u_modl;
uniform mat4 u_translation;

void main() {
	coord = a_coord;
	tex_coord = a_tex_coord;
	color = vec3(a_color);
	gl_Position = u_proj * u_modl * u_translation * vec4(a_coord, 1.0);
}

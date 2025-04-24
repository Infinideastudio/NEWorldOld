#version 330 core

layout(location = 0) in vec3 a_coord;
#ifdef MERGE_FACE
layout(location = 1) in vec3 a_tex_coord;
#else
layout(location = 1) in vec2 a_tex_coord;
#endif
layout(location = 2) in vec3 a_color;

out vec3 coord;
out vec3 tex_coord;
out vec3 color;

uniform mat4 u_proj;
uniform mat4 u_modl;
uniform mat4 u_translation;

void main() {
	coord = a_coord;
#ifdef MERGE_FACE
	tex_coord = a_tex_coord;
#else
	tex_coord = vec3(a_tex_coord, 0.0);
#endif
	color = a_color;
	gl_Position = u_proj * u_modl * u_translation * vec4(a_coord, 1.0);
}

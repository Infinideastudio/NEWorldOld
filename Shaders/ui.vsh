#version 330 core

layout(location = 0) in vec2 a_coord;
layout(location = 1) in vec3 a_tex_coord;
layout(location = 2) in vec4 a_color;

out vec3 tex_coord;
out vec4 color;

uniform float u_buffer_width;
uniform float u_buffer_height;

void main() {
	tex_coord = a_tex_coord;
	color = a_color;
	gl_Position = vec4(a_coord.x / u_buffer_width * 2.0 - 1.0, 1.0 - a_coord.y / u_buffer_height * 2.0, 0.0, 1.0);
}

#version 330 core

in vec3 coord;
in vec3 tex_coord;

layout(location = 0) out vec4 o_frag_color;

uniform sampler2DArray u_diffuse;

void main() {
	// Texture color
	vec4 texel = texture(u_diffuse, tex_coord.stp);
	if (texel.a < 0.5) discard;

	o_frag_color = vec4(1.0);
}

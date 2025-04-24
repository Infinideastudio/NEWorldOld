#version 330 core

in vec3 coord;
in vec3 tex_coord;

layout(location = 0) out vec4 o_frag_color;

uniform sampler2D u_texture;
uniform sampler3D u_texture_3d;

void main() {
	// Texture color
#ifdef MERGE_FACE
	vec4 texel = texture(u_texture_3d, tex_coord.stp);
#else
	vec4 texel = texture(u_texture, tex_coord.st);
#endif
	if (texel.a < 0.5) discard;

	o_frag_color = vec4(1.0);
}

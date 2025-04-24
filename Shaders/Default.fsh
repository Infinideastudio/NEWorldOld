#version 330 core

in vec3 coord;
in vec3 tex_coord;
in vec3 color;

layout(location = 0) out vec4 o_frag_color;

uniform sampler2D u_diffuse;
uniform sampler3D u_diffuse_3d;

void main() {
#ifdef MERGE_FACE
	vec4 texel = texture(u_diffuse_3d, tex_coord.stp);
#else
	vec4 texel = texture(u_diffuse, tex_coord.st);
#endif
	if (texel.a <= 0.0) discard;

	o_frag_color = vec4(color, 1.0) * texel;
}

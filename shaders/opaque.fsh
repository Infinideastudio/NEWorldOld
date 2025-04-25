#version 330 core

in vec3 coord;
in vec3 tex_coord;
in vec3 color;
in vec3 normal;
in float block_id;

layout(location = 0) out vec4 o_frag_color;
layout(location = 1) out vec4 o_normal;
layout(location = 2) out vec4 o_block_id;

uniform sampler2DArray u_diffuse;

const float GAMMA = 2.2;

vec2 encode_u16(int v) {
	int high = v / 256;
	int low = v - high * 256;
	return vec2(float(high) / 255.0, float(low) / 255.0);
}

void main() {
	int block_id_i = int(block_id + 0.5);

	// Texture color
	vec4 texel = texture(u_diffuse, tex_coord.stp);
	texel.rgb = pow(texel.rgb, vec3(GAMMA));
	if (texel.a < 0.5) discard;
	texel.a = 1.0;

	o_frag_color = vec4(color, 1.0) * texel;
	o_normal = vec4(normal * 0.5 + vec3(0.5), 1.0);
	o_block_id = vec4(encode_u16(block_id_i), 0.0, 1.0);
}

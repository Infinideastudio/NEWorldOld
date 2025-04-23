#version 110

##NEWORLD_SHADER_DEFINES MergeFace MERGE_FACE

uniform sampler2D Texture;
uniform sampler3D Texture3D;

const float Gamma = 2.2;

void main() {
	// Texture color
#ifdef MERGE_FACE
	vec4 texel = texture3D(Texture3D, gl_TexCoord[0].stp);
#else
	vec4 texel = texture2D(Texture, gl_TexCoord[0].st);
#endif
	texel.rgb = pow(texel.rgb, vec3(Gamma));
	if (texel.a < 0.5) discard;
	texel.a = 1.0;

	gl_FragColor = texel;
}

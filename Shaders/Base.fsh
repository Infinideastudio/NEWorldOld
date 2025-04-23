#version 110

##NEWORLD_SHADER_DEFINES MergeFace MERGE_FACE

uniform sampler2D Texture;
uniform sampler3D Texture3D;

varying vec4 vertexCoords;
varying float blockIDf;

varying vec3 normal;
varying vec4 cameraSpacePosition;
varying vec4 screenSpacePosition;

const float Gamma = 2.2;

vec2 encodeu16(int v) {
	int high = v / 256;
	int low = v - high * 256;
	return vec2(float(high) / 255.0, float(low) / 255.0);
}

void main() {
	int blockID = int(blockIDf + 0.5);

	// Texture color
#ifdef MERGE_FACE
	vec4 texel = texture3D(Texture3D, gl_TexCoord[0].stp);
#else
	vec4 texel = texture2D(Texture, gl_TexCoord[0].st);
#endif
	texel.rgb = pow(texel.rgb, vec3(Gamma));
	if (texel.a < 0.5) discard;
	texel.a = 1.0;

	vec4 color = gl_Color * texel;

	gl_FragData[0] = color;
	gl_FragData[1] = vec4(normal * 0.5 + vec3(0.5), 1.0);
	gl_FragData[2] = vec4(encodeu16(blockID), 0.0, 1.0);
}

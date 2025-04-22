#version 110

##NEWORLD_SHADER_DEFINES MergeFace MERGE_FACE

uniform sampler2D Texture;
uniform sampler3D Texture3D;

varying vec4 vertexCoords;
varying float blockIDf;

varying vec3 normal;
varying vec4 cameraSpacePosition;
varying vec4 screenSpacePosition;

const float Pi = 3.141593;
const float Gamma = 2.2;
const float MaxBlockID = 4096.0;
const int LeafID = 8, GlassID = 9, WaterID = 10;

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
	if (blockID == WaterID) {
		texel.a *= 0.01;
	} else if (blockID == LeafID || blockID == GlassID) {
		texel.a = step(0.5, texel.a);
	}

	vec4 color = gl_Color * texel;
	float depth = screenSpacePosition.z / screenSpacePosition.w * 0.5 + 0.5;

	gl_FragData[0] = color;
	gl_FragData[1] = vec4(normal * 0.5 + vec3(0.5), 1.0);
	gl_FragData[2] = vec4(encodeu16(blockID), 0.0, 1.0);
}

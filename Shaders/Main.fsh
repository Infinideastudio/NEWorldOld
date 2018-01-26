#version 110

##NEWORLD_SHADER_DEFINES MergeFace MERGE_FACE

uniform sampler2D Texture;
uniform sampler2DShadow DepthTexture;
uniform sampler3D Texture3D;
uniform mat4 ShadowMapProjection;
uniform mat4 ShadowMapModelView;
uniform mat4 Translation;
uniform vec4 BackgroundColor;
uniform float RenderDistance;
uniform float ShadowMapResolution;
uniform vec3 SunlightDirection;

varying vec4 vertCoords;
varying float blockIDf;

varying vec3 normal;
varying vec4 cameraSpacePosition;

const mat4 Normalization = mat4(
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 0.5, 0.0,
	0.5, 0.5, 0.4999, 1.0
);

const float MaxBlockID = 4096.0;
const int WaterID = 10;

int blockID;

vec2 encodeFloat16(in float v) {
	vec2 enc = fract(vec2(1.0, 255.0) * v);
	enc -= enc.yy * vec2(1.0 / 255.0, 0.0);
	return enc;// + vec2(0.1, 0.1) / 255.0;
}

vec3 encodeFloat24(in float v) {
	vec3 enc = fract(vec3(1.0, 255.0, 255.0 * 255.0) * v);
	enc -= enc.yzz * vec3(1.0 / 255.0, 1.0 / 255.0, 0.0);
	return enc;// + vec3(0.1, 0.1, 0.1) / 255.0;
}

void main() {
	
	mat4 trans = Normalization * ShadowMapProjection * ShadowMapModelView * Translation;
	vec4 rel = gl_ModelViewMatrix * vertCoords;
	vec4 shadowCoords = trans * vertCoords;
	float shadow = 0.0;
	float dist = length(rel);
	
	blockID = int(blockIDf + 0.5);
	
	// Shadow calculation
	if (dot(normal, SunlightDirection) > 0.0) shadow = 0.0;
	else if (shadowCoords.x >= 0.0 && shadowCoords.x <= 1.0 &&
			 shadowCoords.y >= 0.0 && shadowCoords.y <= 1.0 && shadowCoords.z <= 1.0) {
		float xpos = shadowCoords.x * ShadowMapResolution, ypos = shadowCoords.y * ShadowMapResolution, depth = shadowCoords.z;
		float x0 = float(int(xpos)), y0 = float(int(ypos));
		float x1 = x0 + 1.0, y1 = y0 + 1.0;
		float texel00 = shadow2D(DepthTexture, vec3(x0 / ShadowMapResolution, y0 / ShadowMapResolution, depth)).z;
		float texel01 = shadow2D(DepthTexture, vec3(x0 / ShadowMapResolution, y1 / ShadowMapResolution, depth)).z;
		float texel10 = shadow2D(DepthTexture, vec3(x1 / ShadowMapResolution, y0 / ShadowMapResolution, depth)).z;
		float texel11 = shadow2D(DepthTexture, vec3(x1 / ShadowMapResolution, y1 / ShadowMapResolution, depth)).z;
		float w00 = (x1 - xpos) * (y1 - ypos), w01 = (x1 - xpos) * (ypos - y0), w10 = (xpos - x0) * (y1 - ypos), w11 = (xpos - x0) * (ypos - y0);
		shadow = texel00 * w00 + texel01 * w01 + texel10 * w10 + texel11 * w11;
	}
	else shadow = 1.0;
	
	shadow = shadow * clamp(-dot(normal, SunlightDirection) + 0.2, 0.0, 1.0);
	
	// Texture color
#ifdef MERGE_FACE
	vec4 texel = texture3D(Texture3D, gl_TexCoord[0].stp);
#else
	vec4 texel = texture2D(Texture, gl_TexCoord[0].st);
#endif
	
	vec4 color = vec4(
		texel.r * (shadow * 0.3 * 1.8 + 0.7),
		texel.g * (shadow * 0.3 * 1.8 + 0.7),
		texel.b * (shadow * 0.3 * 0.8 + 0.7),
		texel.a
	) * gl_Color;
	
	// Fog calculation & Final color
	//if (color.a < 0.99) color = vec4(color.rgb, mix(1.0, 0.3, clamp((RenderDistance * 0.5 - dist) / 64.0, 0.0, 1.0)));
	
	//if (blockID == WaterID) color.a = 0.0;
	
	vec2 blockIDEncoded = encodeFloat16((blockIDf + 0.1) / MaxBlockID);
	
	vec3 xpos = encodeFloat24((cameraSpacePosition.x + RenderDistance) / 2.0 / RenderDistance);
	vec3 ypos = encodeFloat24((cameraSpacePosition.y + RenderDistance) / 2.0 / RenderDistance);
	vec3 zpos = encodeFloat24((cameraSpacePosition.z + RenderDistance) / 2.0 / RenderDistance);
	
	gl_FragData[0] = vec4(mix(BackgroundColor.rgb, color.rgb, clamp((RenderDistance - dist) / 32.0, 0.0, 1.0)), color.a);
	gl_FragData[1] = vec4(normal * 0.5 + vec3(0.5, 0.5, 0.5), 1.0);
	gl_FragData[2] = vec4(blockIDEncoded, 0.0, 1.0);
	gl_FragData[3] = vec4(xpos, 1.0);
	gl_FragData[4] = vec4(ypos, 1.0);
	gl_FragData[5] = vec4(zpos, 1.0);
}

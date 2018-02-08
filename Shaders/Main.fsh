#version 120

##NEWORLD_SHADER_DEFINES MergeFace MERGE_FACE

uniform sampler2D Texture;
uniform sampler2DShadow DepthTexture;
uniform sampler3D Texture3D;
uniform mat4 ShadowMapProjection;
uniform mat4 ShadowMapModelView;
uniform mat4 Translation;
uniform vec4 BackgroundColor;
uniform float RenderDistance;
uniform float ShadowDistance;
uniform float ShadowMapResolution;
uniform vec3 SunlightDirection;
uniform float GameTime;
uniform ivec3 PlayerPositionInt;
uniform vec3 PlayerPositionFrac;

varying vec4 vertCoords;
varying float blockIDf;

varying vec3 normal;
varying vec4 cameraSpacePosition;
varying vec4 screenSpacePosition;

const mat4 Normalization = mat4(
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 0.5, 0.0,
	0.5, 0.5, 0.4999, 1.0
);

const float MaxBlockID = 4096.0;
const int WaterID = 10, GlowStoneID = 12;

const int RepeatLength = 1024;

int blockID;

vec2 encodeFloat16(in float v) {
	vec2 enc = fract(vec2(1.0 - 1.0 / 1024.0, 255.0) * v);
	enc -= enc.yy * vec2(1.0 / 255.0, 0.0);
	return enc;
}

vec3 encodeFloat24(in float v) {
	vec3 enc = fract(vec3(1.0 - 1.0 / 1024.0, 255.0, 255.0 * 255.0) * v);
	enc -= enc.yzz * vec3(1.0 / 255.0, 1.0 / 255.0, 0.0);
	return enc;
}

int iMod(int v, int m) {
	int res = v - v / m * m;
	if (res < 0) return res + m;
	return res;
}

float distToEdge(in vec2 v) {
	return min(min(v.x, 1.0 - v.x), min(v.y, 1.0 - v.y));
}

const float Pi = 3.141593;
float interpolate(float a, float b, float x) {
	return a * (1.0 - (1.0 - cos(x * Pi)) * 0.5) + b * (1.0 - cos(x * Pi)) * 0.5;
	//return a * (1.0 - 3.0 * x * x + 2.0 * x * x * x) + b * (3 * x * x - 2 * x * x * x);
}

float repeatedNoise3D(vec3 c) {
	c = mod(c, float(RepeatLength));
    return fract(sin(dot(c.xyz, vec3(12.9898, 78.233, 233.666))) * 43758.5453);
}

float interpolatedNoise3D(in vec3 c) {
	int int_X, int_Y, int_Z;
	float fractional_X, fractional_Y, fractional_Z, e1, e2, e3, e4, i1, i2;
	int_X = int(floor(c.x));
	fractional_X = fract(c.x);
	int_Y = int(floor(c.y));
	fractional_Y = fract(c.y);
	int_Z = int(floor(c.z));
	fractional_Z = fract(c.z);
	vec3 v = vec3(floor(c.x), floor(c.y), floor(c.z));
	e1 = interpolate(repeatedNoise3D(v + vec3(0.0, 0.0, 0.0)), repeatedNoise3D(v + vec3(1.0, 0.0, 0.0)), fractional_X);
	e2 = interpolate(repeatedNoise3D(v + vec3(0.0, 1.0, 0.0)), repeatedNoise3D(v + vec3(1.0, 1.0, 0.0)), fractional_X);
	e3 = interpolate(repeatedNoise3D(v + vec3(0.0, 0.0, 1.0)), repeatedNoise3D(v + vec3(1.0, 0.0, 1.0)), fractional_X);
	e4 = interpolate(repeatedNoise3D(v + vec3(0.0, 1.0, 1.0)), repeatedNoise3D(v + vec3(1.0, 1.0, 1.0)), fractional_X);
	i1 = interpolate(e1, e2, fractional_Y);
	i2 = interpolate(e3, e4, fractional_Y);
	return interpolate(i1, i2, fractional_Z);
}

float perlinNoise3D(in vec3 c, in float lod) {
	float total = 0.0, frequency = 1.0, amplitude = 1.0, step = 0.5 * min(lod, 1.0);
	for (int i = 0; i < 4; i++) {
		total += interpolatedNoise3D(c * frequency) * amplitude;
		frequency *= 2.0;
		amplitude *= step;
	}
	return total * (1.0 - step);
}

float getWaveHeight(in vec2 v, in float lod) {
	float t = GameTime * 0.02;
	return perlinNoise3D(vec3(v, 0.0) + vec3(0.8 * t, 0.6 * t, t), lod);
}

vec3 getWaveNormal(in vec3 v, in float lod) {
	float hv = getWaveHeight(v.xz, lod);
	float xd = (getWaveHeight(v.xz + vec2(0.01, 0.0), lod) - hv) * 4.0;
	float zd = (getWaveHeight(v.xz + vec2(0.0, 0.01), lod) - hv) * 4.0;
	float yd = sqrt(1.0 - xd * xd - zd * zd);
	return vec3(xd, yd, zd);
}

float sampleShadow(ivec2 pixel, float depth) {
	return shadow2D(DepthTexture, vec3(float(pixel.x) / ShadowMapResolution, float(pixel.y) / ShadowMapResolution, depth)).z;
}

void main() {
	mat4 trans = Normalization * ShadowMapProjection * ShadowMapModelView * Translation;
	vec4 rel = gl_ModelViewMatrix * vertCoords;
	vec4 shadowCoords = trans * vertCoords;
	float luminance = 0.0, sunlight = 0.0;
	float dist = length(rel);
	
	blockID = int(blockIDf + 0.5);
	ivec3 PlayerPositionMod = ivec3(
		iMod(PlayerPositionInt.x, RepeatLength),
		iMod(PlayerPositionInt.y, RepeatLength),
		iMod(PlayerPositionInt.z, RepeatLength)
	);
	
	// Shadow calculation
	if (dot(normal, SunlightDirection) > 0.0) sunlight = 0.0;
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
		sunlight = texel00 * w00 + texel01 * w01 + texel10 * w10 + texel11 * w11;
		
		
		float factor = clamp(min(distToEdge(shadowCoords.xy) * 10.0, (ShadowDistance - dist) / ShadowDistance * 10.0), 0.0, 1.0);
		sunlight = mix(0.8, sunlight, factor);
	}
	else sunlight = 0.8;
	
	luminance = clamp((-dot(normal, SunlightDirection) * 0.5 + 0.5) * 0.3 + 0.4 + sunlight * 0.5, 0.0, 1.1);
	
	vec4 color = gl_Color;
	
	if (blockID == GlowStoneID) {
		luminance = 1.0;
		color = vec4(1.0);
	}
	
	// Texture color
#ifdef MERGE_FACE
	vec4 texel = texture3D(Texture3D, gl_TexCoord[0].stp);
#else
	vec4 texel = texture2D(Texture, gl_TexCoord[0].st);
#endif
	
	color *= vec4(texel.rgb * luminance, texel.a);
	
	// Fog calculation & Final color
	
	vec3 finalNormal = normal;
	
	if (blockID == WaterID) {
		if (normal.y > 0.1) {
			vec4 position = Translation * vertCoords + vec4(vec3(PlayerPositionMod) + PlayerPositionFrac, 0.0);
			vec3 waveNormal = getWaveNormal(position.xyz, max(0.0, 2.0 - dist * 0.05));
			finalNormal = waveNormal;
		}
	}
	
	vec2 blockIDEncoded = encodeFloat16((blockIDf + 0.1) / MaxBlockID);
	float depth = screenSpacePosition.z / screenSpacePosition.w * 0.5 + 0.5;
	
	gl_FragData[0] = color;
	gl_FragData[1] = vec4(finalNormal * 0.5 + vec3(0.5, 0.5, 0.5), 1.0);
	gl_FragData[2] = vec4(blockIDEncoded, 0.0, 1.0);
	gl_FragData[3] = vec4(encodeFloat24(depth), 1.0);
}

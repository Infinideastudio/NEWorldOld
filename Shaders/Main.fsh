#version 120

##NEWORLD_SHADER_DEFINES MergeFace MERGE_FACE

//#define WISDOM_SHADERS_WAVES

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

const mat4 Normalization = mat4(
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 0.5, 0.0,
	0.5, 0.5, 0.4999, 1.0
);

const float MaxBlockID = 4096.0;
const int WaterID = 10;

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

int iMod(int v, int mod) {
	int res = v - v / mod * mod;
	if (res < 0) return res + mod;
	return res;
}

float distToEdge(in vec2 v) {
	return min(min(v.x, 1.0 - v.x), min(v.y, 1.0 - v.y));
}

float noise(vec2 c) {
    return fract(sin(dot(c.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

///*
const float Pi = 3.141593;
float interpolate(float a, float b, float x) {
	return a * (1.0 - (1.0 - cos(x * Pi)) * 0.5) + b * (1.0 - cos(x * Pi)) * 0.5;
	//return a * (1.0 - 3.0 * x * x + 2.0 * x * x * x) + b * (3 * x * x - 2 * x * x * x);
}
//*/

//#define interpolate mix

float interpolatedNoise(in vec2 c) {
	int int_X, int_Y;
	float fractional_X, fractional_Y, v1, v2, v3, v4, i1, i2;
	int_X = int(floor(c.x));
	fractional_X = c.x - floor(c.x);
	int_Y = int(floor(c.y));
	fractional_Y = c.y - floor(c.y);
	vec2 v = vec2(floor(c.x), floor(c.y));
	v1 = noise(v + vec2(0.0, 0.0));
	v2 = noise(v + vec2(1.0, 0.0));
	v3 = noise(v + vec2(0.0, 1.0));
	v4 = noise(v + vec2(1.0, 1.0));
	i1 = interpolate(v1, v2, fractional_X);
	i2 = interpolate(v3, v4, fractional_X);
	return interpolate(i1, i2, fractional_Y);
}

float perlinNoise2D(in vec2 c, in float lod) {
	float total = 0.0, frequency = 1.0, amplitude = 1.0, step = 0.5 * min(lod, 1.0);
	for (int i = 0; i < 4; i++) {
		total += interpolatedNoise(c * frequency) * amplitude;
		frequency *= 2.0;
		amplitude *= step;
	}
	return total * (1.0 - step);
}

#ifdef WISDOM_SHADERS_WAVES

float frameTimeCounter = GameTime * 0.05;

// sea
#define SEA_HEIGHT 0.2 // [0.1 0.2 0.3]

#define NATURAL_WAVE_GENERATOR

#ifdef NATURAL_WAVE_GENERATOR
const int ITER_GEOMETRY = 3;
const int ITER_GEOMETRY2 = 4;

float sea_octave_micro(vec2 uv, float choppy) {
	uv.x += interpolatedNoise(uv) - 0.5;
	uv.y += interpolatedNoise(uv + vec2(2333.0, 6666.0)) - 0.5;
	vec2 wv = 1.0-abs(sin(uv));
	vec2 swv = abs(cos(uv));
	wv = mix(wv,swv,wv);
	return pow(1.0-pow(wv.x * wv.y,0.75),choppy);
}
#else
const int ITER_GEOMETRY = 3;
const int ITER_GEOMETRY2 = 3;

float sea_octave_micro(vec2 uv, float choppy) {
	uv += noise2(uv);
	return (1.0 - sin(uv.x)) * cos(1.0 - uv.y) * 0.7;
}
#endif
const float SEA_CHOPPY = 4.5;
const float SEA_SPEED = 0.8;
const float SEA_FREQ = 0.12;
const mat2 octave_m = mat2(1.4,1.1,-1.2,1.4);

const float height_mul[4] = float[4] (
	0.32, 0.24, 0.20, 0.22
);
const float total_height =
  height_mul[0] + 
  height_mul[0] * height_mul[1] +
  height_mul[0] * height_mul[1] * height_mul[2] +
  height_mul[0] * height_mul[1] * height_mul[2] * height_mul[3];
const float rcp_total_height = 1.0 / total_height;

float getwave(vec3 p, in float lod) {
	float freq = SEA_FREQ;
	float amp = SEA_HEIGHT;
	float choppy = SEA_CHOPPY;
	vec2 uv = p.xz - vec2(frameTimeCounter * 0.5, 0.0); uv.x *= 0.75;

	float wave_speed = frameTimeCounter * SEA_SPEED;

	float d, h = 0.0;
	for(int i = 0; i < ITER_GEOMETRY; i++) {
		d = sea_octave_micro((uv+wave_speed)*freq,choppy);
		h += d * amp;
		uv *= octave_m; freq *= 1.9; amp *= height_mul[i]; wave_speed *= -1.1;
		choppy = mix(choppy,1.0,0.2);
	}

	return (h * rcp_total_height) * lod;
}

float getwave2(vec3 p, in float lod) {
	float freq = SEA_FREQ;
	float amp = SEA_HEIGHT;
	float choppy = SEA_CHOPPY;
	vec2 uv = p.xz - vec2(frameTimeCounter * 0.5, 0.0); uv.x *= 0.75;

	float wave_speed = frameTimeCounter * SEA_SPEED;

	float d, h = 0.0;
	for(int i = 0; i < ITER_GEOMETRY2; i++) {
		d = sea_octave_micro((uv+wave_speed)*freq,choppy);
		h += d * amp;
		uv *= octave_m; freq *= 1.9; amp *= height_mul[i]; wave_speed *= -1.1;
		choppy = mix(choppy,1.0,0.2);
	}

	return (h * rcp_total_height) * lod;
}

float getWaveHeight(in vec2 v) {
	return getwave2(vec3(v.x, 1.0, v.y), 1.0);
}

vec3 getWaveNormal(in vec3 wwpos, in float lod) {
	float displacement = getwave2(wwpos, lod);
	vec3 dir = vec3(0.0, 1.0, 0.0);
	vec3 w1 = vec3(0.01, dir.y * getwave2(wwpos + vec3(0.01, 0.0, 0.0), lod), 0.0);
	vec3 w2 = vec3(0.0, dir.y * getwave2(wwpos + vec3(0.0, 0.0, 0.01), lod), 0.01);
	vec3 w0 = displacement * dir;
	#define tangent w1 - w0
	#define bitangent w2 - w0
	return normalize(cross(bitangent, tangent));
}

#else

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

#endif

void main() {
	
	mat4 trans = Normalization * ShadowMapProjection * ShadowMapModelView * Translation;
	vec4 rel = gl_ModelViewMatrix * vertCoords;
	vec4 shadowCoords = trans * vertCoords;
	float shadow = 0.0;
	float dist = length(rel);
	
	blockID = int(blockIDf + 0.5);
	ivec3 PlayerPositionMod = ivec3(
		iMod(PlayerPositionInt.x, RepeatLength),
		iMod(PlayerPositionInt.y, RepeatLength),
		iMod(PlayerPositionInt.z, RepeatLength)
	);
	
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
		
		float factor = clamp(min(distToEdge(shadowCoords.xy) * 10.0, (ShadowDistance - dist) / ShadowDistance * 5.0), 0.0, 1.0);
		shadow = mix(0.8, shadow, factor);
	}
	else shadow = 0.8;
	
	shadow = shadow * clamp(-dot(normal, SunlightDirection) + 0.3, 0.0, 1.0);
	
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
	
	vec3 finalNormal = normal;
	
	if (blockID == WaterID) {
		if (normal.y > 0.1) {
			vec4 position = Translation * vertCoords + vec4(vec3(PlayerPositionMod) + PlayerPositionFrac, 0.0);
			vec3 waveNormal = getWaveNormal(position.xyz, max(0.0, 2.0 - dist * 0.05));
			finalNormal = waveNormal;
		}
	}
	
	vec2 blockIDEncoded = encodeFloat16((blockIDf + 0.1) / MaxBlockID);
	
	vec3 xpos = encodeFloat24((cameraSpacePosition.x + RenderDistance) / 2.0 / RenderDistance);
	vec3 ypos = encodeFloat24((cameraSpacePosition.y + RenderDistance) / 2.0 / RenderDistance);
	vec3 zpos = encodeFloat24((cameraSpacePosition.z + RenderDistance) / 2.0 / RenderDistance);
	
	gl_FragData[0] = vec4(mix(BackgroundColor.rgb, color.rgb, clamp((RenderDistance - dist) / 32.0, 0.0, 1.0)), color.a);
	gl_FragData[1] = vec4(finalNormal * 0.5 + vec3(0.5, 0.5, 0.5), 1.0);
	gl_FragData[2] = vec4(blockIDEncoded, 0.0, 1.0);
	gl_FragData[3] = vec4(xpos, 1.0);
	gl_FragData[4] = vec4(ypos, 1.0);
	gl_FragData[5] = vec4(zpos, 1.0);
}

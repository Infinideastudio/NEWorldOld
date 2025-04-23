#version 110

##NEWORLD_SHADER_DEFINES MergeFace MERGE_FACE
##NEWORLD_SHADER_DEFINES VolumetricClouds VOLUMETRIC_CLOUDS

uniform sampler2D Texture0;
uniform sampler2D Texture1;
uniform sampler2D Texture2;
uniform sampler2D DepthTexture;
uniform sampler2DShadow ShadowTexture;
uniform sampler2D NoiseTexture;

uniform float ScreenWidth;
uniform float ScreenHeight;
uniform float BufferSize;
uniform mat4 ProjectionMatrix;
uniform mat4 ModelViewMatrix;
uniform mat4 ProjectionInverse;
uniform mat4 ModelViewInverse;

uniform float RenderDistance;
uniform float ShadowDistance;
uniform mat4 ShadowMapProjection;
uniform mat4 ShadowMapModelView;
//uniform float ShadowMapDepthRange;
uniform float ShadowMapResolution;
uniform float ShadowFisheyeFactor;

uniform vec3 SunlightDirection;
uniform float GameTime;
uniform ivec3 PlayerPositionInt;
uniform vec3 PlayerPositionFrac;

const float Pi = 3.141593;
const float Gamma = 2.2;
const float MaxBlockID = 4096.0;
const int LeafID = 8, GlassID = 9, WaterID = 10, GlowstoneID = 12, IceID = 15, IronID = 17;
const int RepeatLength = 1024;

// Shadow map
const mat4 Normalization = mat4(
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 0.5, 0.0,
	0.5, 0.5, 0.5, 1.0
);
const float ShadowUnits = 32.0;
const float ShadowSmooth = 0.1;
const int ShadowSamples = 16;

// Water reflection
const float WaveUnits = 32.0;
const vec2 WaveScale = vec2(2.0, 2.0);
const int MaxIterations = 32;
const float StepScale = 2.0 / 28.0;

// Volumetric clouds
const float CloudUnits = 32.0; // 1.0 / 16.0;
const vec3 CloudScale = vec3(100.0, 80.0, 100.0);
const float CloudBottom = 100.0, CloudTop = 65536.0, CloudTransition = 120.0;
const int CloudIterations = 32;
const float CloudStepScale = 16.0;

float xScale, yScale, xRange, yRange;
ivec3 PlayerPositionMod;

float rand2(vec2 v) {
	return fract(sin(dot(v, vec2(12.9898, 78.233))) * 43758.5453);
}

int decodeu16(vec2 v) {
	int high = int(v.x * 255.0 + 0.5);
	int low = int(v.y * 255.0 + 0.5);
	return high * 256 + low;
}

vec3 divide(vec4 v) {
	return (v / v.w).xyz;
}

vec3 screenSpaceToTextureCoords(vec4 v) {
	vec3 res = divide(v) / 2.0 + vec3(0.5);
	res.x *= xRange;
	res.y *= yRange;
	return res;
}

float getTextureDepth(vec2 texCoord) {
	return texture2D(DepthTexture, texCoord).r * 2.0 - 1.0;
}

vec3 getTexturePosition(vec2 texCoord) {
	float depth = getTextureDepth(texCoord);
	texCoord = vec2(texCoord.x / xRange, texCoord.y / yRange) * 2.0 - vec2(1.0);
	vec4 res = ProjectionInverse * vec4(texCoord, depth, 1.0);
	return res.xyz / res.w;
}

vec4 getColor(vec2 texCoord) {
	return texture2D(Texture0, texCoord);
}

vec3 getNormal(vec2 texCoord) {
	return normalize(texture2D(Texture1, texCoord).rgb * 2.0 - vec3(1.0));
}

int getBlockID(vec2 texCoord) {
	return decodeu16(texture2D(Texture2, texCoord).rg);
}

float distToEdge(vec2 v) {
	v.x *= xScale;
	v.y *= yScale;
	return min(min(v.x, 1.0 - v.x), min(v.y, 1.0 - v.y));
}

vec2 fisheyeProjection(vec2 position) {
	float dist = length(position);
	float distortFactor = (1.0 - ShadowFisheyeFactor) + dist * ShadowFisheyeFactor;
	position /= distortFactor;
	return position;
}

vec2 fisheyeInverse(vec2 position) {
	float dist = length(position);
	float distortFactor = (1.0 - dist * ShadowFisheyeFactor) / (1.0 - ShadowFisheyeFactor);
	position /= distortFactor;
	return position;
}

float interpolatedNoise(vec3 x) {
	const vec2 DefaultOffset = vec2(37.0, 17.0);
	vec3 p = floor(x);
	vec3 f = fract(x);
//	f = smoothstep(0.0, 1.0, f);
	vec2 uv = (p.xy + DefaultOffset * p.z) + f.xy;
	vec2 v = texture2D(NoiseTexture, uv / 256.0).rb;
	return mix(v.x, v.y, f.z);
}

float sampleShadow(vec4 coords, float bias) {
	coords.z -= bias;
	coords = Normalization * coords;
	return shadow2D(ShadowTexture, divide(coords)).z;
}

float shadow(vec3 coords, float slope /* vec3 normal */) {
	// Shadow calculation
	vec4 shadowCoords = vec4(coords, 1.0);
	shadowCoords = ShadowMapProjection * ShadowMapModelView * shadowCoords;
	shadowCoords = vec4(fisheyeProjection(shadowCoords.xy), shadowCoords.zw);

	if (shadowCoords.x >= -1.0 && shadowCoords.x < 1.0 && shadowCoords.y >= -1.0 && shadowCoords.y < 1.0 && shadowCoords.z < 1.0) {
		float dist = length(ModelViewMatrix * vec4(coords, 1.0));
		float shift = 2.0 / ShadowMapResolution;

		// Neighbor pixel distance in light space
		shift = length(fisheyeInverse(shadowCoords.xy + shift * normalize(shadowCoords.xy)) - fisheyeInverse(shadowCoords.xy));

		// Combined bias
		float bias = slope * shift * 0.4;
		float factor = 1.0 - smoothstep(0.8, 1.0, dist / ShadowDistance);
		return mix(1.0, sampleShadow(shadowCoords, bias), factor);
	}
	return 1.0;
}

vec3 getSkyColor(vec3 dir) {
	dir = normalize(dir);
	vec3 tangent = normalize(cross(vec3(0.0, 1.0, 0.0), -SunlightDirection));
	vec3 bitangent = cross(tangent, -SunlightDirection);
	vec3 local = vec3(dot(dir, tangent), dot(dir, bitangent), dot(dir, -SunlightDirection));
	if (abs(local.x) < 0.03 && abs(local.y) < 0.03 && local.z > 0.0) {
		return vec3(3.5, 3.0, 2.9) * 10.0;
	}
	return mix(
		vec3(1.2, 1.6, 2.0),
		vec3(0.25, 0.4, 1.0),
		smoothstep(0.0, 1.0, normalize(dir).y * 2.0)
	) * 1.0;
}

vec4 diffuse(vec2 texCoord) {
	int blockID = getBlockID(texCoord);
	if (blockID == 0) return vec4(0.0);
	vec4 color = getColor(texCoord);
	vec3 normal = getNormal(texCoord);
	vec3 tangent = normalize(cross(normal, vec3(1.0, 1.0, 1.0)));
	vec3 bitangent = cross(normal, tangent);
	vec3 translation = vec3(PlayerPositionMod) + PlayerPositionFrac;

	vec3 cameraSpacePosition = getTexturePosition(texCoord);
	vec4 screenSpacePosition = ProjectionMatrix * vec4(cameraSpacePosition, 1.0);
	vec3 worldSpacePosition = divide(ModelViewInverse * vec4(cameraSpacePosition, 1.0)) + translation;
	vec3 shadowCoords = floor(worldSpacePosition * ShadowUnits + normal * 0.5) / ShadowUnits - translation;

	float sunlight = 0.0;
	float cosTheta = dot(normal, -SunlightDirection);
	float slope = sqrt(1.0 - cosTheta * cosTheta) / cosTheta;

	/*
	// Simulate subsurface scattering for certain blocks
	if (cosTheta <= 0.0) {
		shadowCoords -= SunlightDirection * 0.3;
		slope = 0.0;
	}
	*/

	// Sample shadow map
	if (blockID != WaterID && cosTheta > 0.0) {
		for (int i = 0; i < ShadowSamples; i++) {
			float ratio = float(i) / float(ShadowSamples);
			vec3 offset = (
				rand2(gl_FragCoord.xy + vec2(ratio, ratio)) * normal +
				(2.0 * rand2(gl_FragCoord.xy + vec2(ratio, 0.0)) - vec3(1.0)) * tangent +
				(2.0 * rand2(gl_FragCoord.xy + vec2(0.0, ratio)) - vec3(1.0)) * bitangent) * ShadowSmooth;
			sunlight += shadow(shadowCoords + offset, slope);
		}
		sunlight /= float(ShadowSamples);
		sunlight *= cosTheta;
	}

	float glow = 0.0;
	if (blockID == GlowstoneID) glow = 5.0;

	return vec4(max(vec3(3.5, 3.0, 2.9) * sunlight + vec3(0.35, 0.56, 0.96) * 0.5, glow) * color.rgb, color.a);
}

vec3 diffuseBackground(vec2 texCoord) {
	vec3 viewDir = normalize(divide(ModelViewInverse * ProjectionInverse * vec4(texCoord / vec2(xRange, yRange) * 2.0 - vec2(1.0), -1.0, 1.0)));
	vec3 sky = getSkyColor(viewDir);
	vec4 color = diffuse(texCoord);
	// Fog
	if (getBlockID(texCoord) != 0) {
		vec3 cameraSpacePosition = getTexturePosition(texCoord);
		float dist = length(cameraSpacePosition);
		color.a *= clamp((RenderDistance - dist) / 32.0, 0.0, 1.0);
	}
	return mix(sky, color.rgb, color.a);
}

float waveNoise(vec3 c) {
	float res = 0.0;
	res += interpolatedNoise(c * 1.0) / 1.0;
	res += interpolatedNoise(c * 2.0) / 2.0;
	res += interpolatedNoise(c * vec3(6.0, 6.0, 0.0)) / 4.0;
	res += interpolatedNoise(c * vec3(24.0, 24.0, 0.0)) / 12.0;
	return res / (1.0 + 1.0 / 2.0 + 1.0 / 4.0 + 1.0 / 12.0);
}

float getWaveHeight(vec2 v, float factor) {
	float t = GameTime;
	return waveNoise(vec3((v + vec2(0.01 * t, 0.007 * t)) / WaveScale, 0.001 * t)) * 0.1 * factor;
}

vec3 getWaveNormal(vec3 v, float factor) {
	vec2 d = vec2(0.02) * WaveScale;
	float hv = getWaveHeight(v.xz, factor);
	vec3 xx = vec3(d.x, getWaveHeight(v.xz + vec2(d.x, 0.0), factor) - hv, 0.0);
	vec3 zz = vec3(0.0, getWaveHeight(v.xz + vec2(0.0, d.y), factor) - hv, d.y);
	return normalize(cross(zz, xx));
}

float schlick(float n, float m, float cosTheta) {
	if (cosTheta < 0.0) return 1.0;
	float r0 = pow((n - m) / (n + m), 2.0);
	return r0 + (1.0 - r0) * pow(1.0 - cosTheta, 5.0);
}

vec3 ssr(vec4 org, vec4 dir, vec3 bgColor, bool inside) {
	vec3 org3 = divide(org);
	vec3 dir3 = normalize(divide(org + dir) - org3);
	dir3 /= length(dir3.xy);

	vec3 curr3 = org3;
	float currDist = length(divide(ProjectionInverse * vec4(curr3, 1.0)));
	float step = 1.0;
	bool found = false;
	vec2 best = screenSpaceToTextureCoords(vec4(curr3, 1.0)).xy;
	int self = getBlockID(best);
	float ratio = 1.0;

	for (int i = 0; i < MaxIterations; i++) {
		vec3 next3 = curr3 + dir3 * step * StepScale * (i == 0 ? (1.0 + rand2(gl_FragCoord.xy)) : 1.0);
		if (next3.x >= -1.0 && next3.x < 1.0 && next3.y >= -1.0 && next3.y < 1.0 && next3.z >= -1.0 && next3.z < 1.0) {
			vec2 texCoord = screenSpaceToTextureCoords(vec4(next3, 1.0)).xy;
			float nextDist = next3.z;
			float pixelDist = getTextureDepth(texCoord);
			if (getBlockID(texCoord) != 0 && pixelDist < nextDist && nextDist - pixelDist < abs(nextDist - currDist) * 3.0) {
				if (!found) ratio = float(i) / float(MaxIterations);
				found = true;
				best = texCoord;
				step /= 2.0;
			} else {
				curr3 = next3;
				currDist = nextDist;
			}
		} else {
			step /= 2.0;
		}
	}

	if (!found) return bgColor;
	float factor = smoothstep(0.8, 1.0, max(1.0 - distToEdge(best) * 2.0, ratio));
	return mix(diffuseBackground(best), bgColor, factor);
}

int modi(int v, int m) {
	int res = v - v / m * m;
	if (res < 0) return res + m;
	return res;
}

float cloudNoise(vec3 c) {
	float res = 0.0;
	res += interpolatedNoise(c * 1.0) / 1.0;
	res += interpolatedNoise(c * 2.0) / 2.0;
	res += interpolatedNoise(c * 6.0) / 4.0;
	res += interpolatedNoise(c * 24.0) / 12.0;
	return res / (1.0 + 1.0 / 2.0 + 1.0 / 4.0 + 1.0 / 12.0);
}

float getCloudOpacity(vec3 pos) {
	pos = floor(pos * CloudUnits) / CloudUnits;
	float factor = min(
		smoothstep(CloudBottom, CloudBottom + CloudTransition, pos.y),
		1.0 - smoothstep(CloudTop - CloudTransition, CloudTop, pos.y)
	);
	float opacity = clamp(cloudNoise(pos / CloudScale) * 2.0 - 1.2, 0.0, 1.0);
	return factor * opacity;
}

vec3 cloud(vec3 org, vec3 dir, vec3 bgColor, float dist, vec3 center, float quality) {
	vec3 curr = org;
	dir = normalize(dir);
	vec3 res = vec3(0.0);
	float acc = 1.0;
	
	if (curr.y < CloudBottom) {
		if (dir.y <= 0.0) return bgColor;
		curr += dir * (CloudBottom - curr.y) / dir.y;
	} else if (curr.y > CloudTop) {
		if (dir.y >= 0.0) return bgColor;
		curr += dir * (CloudTop - curr.y) / dir.y;
	}
	
	for (int i = 0; i < CloudIterations; i++) {
		float step = CloudStepScale / quality * (i == 0 ? rand2(gl_FragCoord.xy) : 1.0);
		curr += dir * step;
		
		if (acc < 0.01) break;
		if (length(curr - org) > dist) break;
		
		if (curr.y >= CloudBottom && curr.y <= CloudTop) {
			float factor = 1.0;
			factor *= 1.0 - smoothstep(RenderDistance * 0.8, RenderDistance, length(curr - center));
			factor *= 1.0 - smoothstep(dist * 0.8, dist, length(curr - org));
			float transmittence = pow(1.0 - factor * getCloudOpacity(curr), step);
			if (transmittence < 0.99) {
				float scattering = 1.0;
				scattering *= pow(1.0 - getCloudOpacity(curr - SunlightDirection * 8.0), 8.0);
				scattering *= pow(1.0 - getCloudOpacity(curr - SunlightDirection * 16.0), 8.0);
				scattering *= pow(1.0 - getCloudOpacity(curr - SunlightDirection * 32.0), 16.0);
				// scattering *= pow(1.0 - getCloudOpacity(curr - SunlightDirection * 64.0), 32.0);
				vec3 col = vec3(3.5, 3.0, 2.9) * scattering + vec3(0.25, 0.4, 1.0) * 0.5;
				res += acc * (1.0 - transmittence) * col;
				acc *= transmittence;
			}
		} else {
			break;
		}
	}
	
	return res + acc * bgColor;
}

vec3 aces(vec3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
	xScale = BufferSize / ScreenWidth;
	yScale = BufferSize / ScreenHeight;
	xRange = ScreenWidth / BufferSize;
	yRange = ScreenHeight / BufferSize;
	PlayerPositionMod = ivec3(
		modi(PlayerPositionInt.x, RepeatLength * int(CloudScale.x + 0.5)),
		modi(PlayerPositionInt.y, RepeatLength * int(CloudScale.y + 0.5)),
		modi(PlayerPositionInt.z, RepeatLength * int(CloudScale.z + 0.5))
	);
	vec2 texCoord = gl_TexCoord[0].st * vec2(xRange, yRange);
	int blockID = getBlockID(texCoord);
	
	vec3 cameraSpacePosition = getTexturePosition(texCoord);
	vec4 screenSpacePosition = ProjectionMatrix * vec4(cameraSpacePosition, 1.0);
	
	vec3 viewOrigin = vec3(PlayerPositionMod) + PlayerPositionFrac;
	vec3 viewDir = normalize(divide(ModelViewInverse * ProjectionInverse * vec4(texCoord / vec2(xRange, yRange) * 2.0 - vec2(1.0), -1.0, 1.0)));
	vec3 color = diffuseBackground(texCoord);
	vec3 normal = getNormal(texCoord);
	
	if (blockID == WaterID || blockID == IceID || blockID == IronID) {
		vec3 reflectOrigin = viewOrigin + divide(ModelViewInverse * vec4(cameraSpacePosition, 1.0));
		bool inside = dot(viewOrigin - reflectOrigin, normal) < 0.0;
		
		// Water wave effect
		if (blockID == WaterID && normal.y > 0.9) {
			vec3 waveCoords = floor(reflectOrigin * WaveUnits) / WaveUnits;
			vec3 waveNormal = getWaveNormal(waveCoords, 1.0 - smoothstep(0.0, 0.5, length(cameraSpacePosition) / RenderDistance));

			// Only admit wave if it does not change the relative orientation
			float cosTheta = dot(normalize(viewOrigin - reflectOrigin), waveNormal);
			if (inside) cosTheta = -cosTheta;
			if (cosTheta >= 0.0) normal = waveNormal;
		}

		// Glossy metal effect
		if (blockID == IronID) {
			vec3 perturb = vec3(
				rand2(gl_FragCoord.xy),
				rand2(gl_FragCoord.xy + vec2(0.1, 0.0)),
				rand2(gl_FragCoord.xy + vec2(0.0, 1.0)));
			perturb = (perturb - vec3(0.5)) * 0.03;
			normal = normalize(normal + perturb);
		}

		// Reflection calculation
		vec3 reflectDir = reflect(normalize(reflectOrigin - viewOrigin), normal);
		float cosTheta = dot(normalize(viewOrigin - reflectOrigin), normal);
		if (inside) cosTheta = -cosTheta;
		
		vec3 sky = getSkyColor(reflectDir);
		vec3 reflection;
		if (!inside) {
			reflection = sky;
#ifdef VOLUMETRIC_CLOUDS
			reflection = cloud(reflectOrigin, reflectDir, sky, 65536.0, viewOrigin, 0.5);
#endif
		} else {
			reflection = vec3(0.1);
		}
		vec4 screenSpaceReflectDir = ProjectionMatrix * ModelViewMatrix * vec4(reflectDir, 0.0);
		reflection = ssr(screenSpacePosition, screenSpaceReflectDir, reflection, inside);

		// Fresnel-Schlick blending
		float albedo = 1.0;
		if (!inside) {
			if (blockID == WaterID) albedo *= schlick(1.0, 1.33, cosTheta);
			else if (blockID == IceID) albedo *= schlick(1.0, 2.42, cosTheta);
			else if (blockID == IronID) reflection *= color;
		} else {
			// TODO: make this more realistic
			albedo *= smoothstep(0.0, 1.0, 1.0 - cosTheta * cosTheta);
		}
		color = mix(color, reflection, albedo);
	}
	
	// Fog
	float dist = 65536.0;
	if (blockID != 0) {
		dist = length(cameraSpacePosition);
	}
#ifdef VOLUMETRIC_CLOUDS
	color = cloud(viewOrigin, viewDir, color, dist, viewOrigin, 1.0);
#endif

	// Component-wise tone mapping
	// color = color / (color + vec3(1.0)); // Reinhard tone mapping
	// color = vec3(1.0) - exp(-color * 0.8); // Exposure tone mapping
	color = aces(color * 0.8);

	// Luminance-based tone mapping
	// float luminance = (color.r + color.g + color.b) / 3.0;
	// color /= luminance;
	// luminance = luminance / (luminance + 1.0); // Reinhard tone mapping
	// luminance = 1.0 - exp(-luminance * 0.8); // Exposure tone mapping
	// color *= luminance;
	
	// Gamma encoding
	color = pow(color, vec3(1.0 / Gamma));
	gl_FragColor = vec4(color, 1.0);
	gl_FragDepth = divide(screenSpacePosition).z * 0.5 + 0.5;
}

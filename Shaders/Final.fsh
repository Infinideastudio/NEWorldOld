#version 110

##NEWORLD_SHADER_DEFINES VolumetricClouds VOLUMETRIC_CLOUDS

uniform sampler2D Texture0;
uniform sampler2D Texture1;
uniform sampler2D Texture2;
uniform sampler2D Texture3;
uniform sampler2D NoiseTexture;
uniform float ScreenWidth;
uniform float ScreenHeight;
uniform float BufferSize;
uniform mat4 ProjectionMatrix;
uniform mat4 ModelViewMatrix;
uniform mat4 ProjectionInverse;
uniform mat4 ModelViewInverse;
uniform float RenderDistance;
uniform vec4 BackgroundColor;

uniform vec3 SunlightDirection;
uniform ivec3 PlayerPositionInt;
uniform vec3 PlayerPositionFrac;

const float MaxBlockID = 4096.0;
const int GlassID = 9, WaterID = 10, IceID = 15, IronID = 17;
const int RepeatLength = 1024;

// Water reflection
const int MaxIterations = 50, MaxNearIterations = 12;
const float StepScale = 4.0;

// Volumetric clouds
const vec3 CloudScale = vec3(100.0, 50.0, 100.0);
const float CloudBottom = 160.0, CloudTop = 240.0, CloudTransition = 40.0, CloudRangeMult = 2.0;
const int CloudIterations = 16;
const float CloudStep = 8.0;

float xScale, yScale, xRange, yRange, blockIDf;
int blockID;
vec3 cameraSpacePosition, screenSpacePosition;

float decodeFloat16(in vec2 v) {
	return dot(v, vec2(1.0, 1.0 / 255.0));
}

float decodeFloat24(in vec3 v) {
	return dot(v, vec3(1.0, 1.0 / 255.0, 1.0 / 255.0 / 255.0));
}

vec3 divide(in vec4 v) {
	return (v / v.w).xyz;
}

vec3 cameraSpaceToTextureCoords(in vec3 v) {
	vec4 v4 = ProjectionMatrix * vec4(v, 1.0);
	v4 /= v4.w;
	vec3 v3 = v4.xyz / 2.0 + vec3(0.5, 0.5, 0.5);
	v3.x *= xRange;
	v3.y *= yRange;
	return v3;
}

float getTextureDepth(in vec2 texCoord) {
	return decodeFloat24(texture2D(Texture3, texCoord).rgb) * 2.0 - 1.0;
}

vec3 getTexturePosition(in vec2 texCoord) {
	float depth = getTextureDepth(texCoord);
	texCoord = vec2(texCoord.x / xRange, texCoord.y / yRange) * 2.0 - vec2(1.0);
	vec4 res = ProjectionInverse * vec4(texCoord, depth, 1.0);
	return res.xyz / res.w;
}

int getBlockID(in vec2 texCoord) {
	return int(decodeFloat16(texture2D(Texture2, texCoord).rg) * MaxBlockID + 0.5);
}

float getDist(in vec3 relative) {
	return length(relative);
}

bool inside(in vec2 v) {
	return v.x >= 0.0 && v.x <= xRange && v.y >= 0.0 && v.y <= yRange;
}

float distToEdge(in vec2 v) {
	v.x *= xScale;
	v.y *= yScale;
	return min(min(v.x, 1.0 - v.x), min(v.y, 1.0 - v.y));
}

vec4 getSkyColor(in vec3 dir) {
	return mix(
		//vec4(240.0 / 255.0, 244.0 / 255.0, 250.0 / 255.0, 1.0),
		vec4(152.0 / 255.0, 211.0 / 255.0, 250.0 / 255.0, 1.0),
		vec4(90.0 / 255.0, 134.0 / 255.0, 206.0 / 255.0, 1.0),
		smoothstep(0.0, 1.0, normalize(dir).y * 2.0)
	);
}

vec3 ssr(in vec3 org, in vec3 dir, in vec3 bgColor, bool underWater) {
	vec3 curr = org, last = org;
	bool found = false;
	float ratio = 1.0, step = clamp(getDist(org) / 10.0, 0.1, 4.0);
	
	dir = normalize(dir);
	
	curr += dir * step * StepScale;
	
	for (int i = 0; i < MaxIterations; i++) {
		vec2 texCoord = cameraSpaceToTextureCoords(curr).xy;
		if (!inside(texCoord)) break;
		
		float currDist = getDist(curr);
		float pixelDist = getDist(getTexturePosition(texCoord));
		
		if (currDist < 0.0 || currDist > RenderDistance) break;
		
		if (getBlockID(texCoord) != 0 && getBlockID(texCoord) != WaterID &&
				pixelDist < currDist && (currDist - pixelDist < step * StepScale * 2.0 + 1.0 || underWater)) {
			found = true;
			dir /= 2.0;
			curr = last;
			ratio = min(ratio, float(i) / float(MaxIterations));
		} else last = curr;
		
		if (i >= MaxNearIterations) step = max(step, 1.0);
		
		curr += dir * step * StepScale;
		
		if (found && (length(dir) < 0.1 || i == MaxIterations - 1)) {
			float factor = clamp(min(distToEdge(texCoord) * 5.0, (1.0 - ratio) * 5.0), 0.0, 1.0);
			return mix(bgColor, texture2D(Texture0, texCoord).rgb, factor);
		}
	}
	
	return bgColor;
}

int iMod(int v, int m) {
	int res = v - v / m * m;
	if (res < 0) return res + m;
	return res;
}

float interpolatedNoise3D(in vec3 x) {
	const vec2 DefaultOffset = vec2(37.0, 17.0);
	vec3 p = floor(x);
	vec3 f = fract(x);
	//f = smoothstep(0.0, 1.0, f);
	vec2 uv = (p.xy + DefaultOffset * p.z) + f.xy;
	vec2 v = texture2D(NoiseTexture, uv / 256.0).rb;
	return mix(v.x, v.y, f.z);
}

float perlinNoise3D(in vec3 c) {
	float res = 0.0;
	res += interpolatedNoise3D(c * 1.0) / 1.0;
	res += interpolatedNoise3D(c * 2.0) / 2.0;
	res += interpolatedNoise3D(c * 4.0) / 4.0;
	res += interpolatedNoise3D(c * 8.0) / 8.0;
	res += interpolatedNoise3D(c * 16.0) / 16.0;
	res += interpolatedNoise3D(c * 32.0) / 32.0;
//	res += interpolatedNoise3D(c * 64.0) / 64.0;
//	res += interpolatedNoise3D(c * 128.0) / 128.0;
	res = clamp(abs(res - 0.5) * 2.0 - 0.8, 0.0, 1.0);
	//res = clamp(abs(res / 2.0 * 2.0 - 1.0) * 2.0 - 0.2, 0.0, 1.0);
	//return res < 0.5 ? 0.0 : 1.0;
	return res < 0.3 ? clamp((res - 0.3) * 4.0 + 0.3, 0.0, 1.0) : res;
}

float getCloudOpacity(in vec3 pos) {
	float factor = min(
		smoothstep(CloudBottom, CloudBottom + CloudTransition, pos.y),
		1.0 - smoothstep(CloudTop - CloudTransition, CloudTop, pos.y)
	);
	return factor * 0.5 * perlinNoise3D(pos / CloudScale);
}

vec4 cloud(in vec3 org, in vec3 dir, in float stp, in float maxDist) {
	vec3 curr = org;
	vec3 res = vec3(0.0);
	float transparency = 1.0;
	
	dir = normalize(dir);
	
	//float stp = 8.0;// / max(dir.y, 0.01);
	
	if (curr.y < CloudBottom) {
		if (dir.y <= 0.0) return vec4(0.0);
		curr += dir * (CloudBottom - curr.y) / dir.y;
	} else if (curr.y > CloudTop) {
		if (dir.y >= 0.0) return vec4(0.0);
		curr += dir * (CloudTop - curr.y) / dir.y;
	}
	
	for (int i = 0; i < CloudIterations; i++) {
		curr += dir * stp;
		
		if (transparency < 0.01) break;
		if (length(curr - org) > maxDist) break;
		
		if (curr.y >= CloudBottom && curr.y <= CloudTop) {
			float factor = smoothstep(-maxDist, -maxDist * 0.6, -length(curr - org));
			float opacity = factor * getCloudOpacity(curr) * 0.5;
			if (opacity > 0.0) {
				float occulusion = getCloudOpacity(curr - SunlightDirection * stp * 4.0);
				float col = clamp(1.2 - occulusion * 0.8, 0.6, 1.0);
				res += transparency * opacity * vec3(col);
				transparency *= 1.0 - opacity;
			}
		} else break;
	}
	
	return vec4(res, 1.0 - transparency);
}

void main() {
	xScale = BufferSize / ScreenWidth;
	yScale = BufferSize / ScreenHeight;
	xRange = ScreenWidth / BufferSize;
	yRange = ScreenHeight / BufferSize;
	
	vec2 texCoord = vec2(gl_TexCoord[0].s * xRange, gl_TexCoord[0].t * yRange);
	
	vec4 data0 = texture2D(Texture0, texCoord);
	vec4 data1 = texture2D(Texture1, texCoord);
	vec4 data2 = texture2D(Texture2, texCoord);
	
	blockIDf = decodeFloat16(data2.rg);
	blockID = int(blockIDf * MaxBlockID + 0.5);
	ivec3 PlayerPositionMod = ivec3(
		iMod(PlayerPositionInt.x, RepeatLength * int(CloudScale.x + 0.5)),
		iMod(PlayerPositionInt.y, RepeatLength * int(CloudScale.y + 0.5)),
		iMod(PlayerPositionInt.z, RepeatLength * int(CloudScale.z + 0.5))
	);
	
	vec4 color;
	
	cameraSpacePosition = getTexturePosition(texCoord);
	screenSpacePosition = divide(ProjectionMatrix * vec4(cameraSpacePosition, 1.0));
	
	float dist = getDist(cameraSpacePosition);
	vec3 viewDir = normalize(divide(ModelViewInverse * ProjectionInverse * vec4(gl_TexCoord[0].st * 2.0 - vec2(1.0), -1.0, 1.0)));
	
	if (blockID == 0) {
		dist = RenderDistance * 4.0;
		color = getSkyColor(viewDir);
	} else if (blockID == WaterID || blockID == IceID || blockID == IronID) {
		vec3 normal = (ModelViewMatrix * vec4(data1.rgb * 2.0 - vec3(1.0, 1.0, 1.0), 0.0)).xyz;
		vec3 reflectDir = reflect(normalize(cameraSpacePosition), normalize(normal));
		float cosTheta = dot(normalize(-cameraSpacePosition), normalize(normal));
		
		vec3 worldReflectDir = (ModelViewInverse * vec4(reflectDir, 0.0)).xyz;
#ifdef VOLUMETRIC_CLOUDS
		vec3 worldPosition = vec3(PlayerPositionInt) + PlayerPositionFrac + divide(ModelViewInverse * vec4(cameraSpacePosition, 1.0));
		vec4 cloudColor = cloud(worldPosition, worldReflectDir, CloudStep * 4.0, RenderDistance * 2.0);
		vec3 bgColor = cloudColor.rgb + (1.0 - cloudColor.a) * getSkyColor(worldReflectDir).rgb;
#else
		vec3 bgColor = getSkyColor(worldReflectDir).rgb;
#endif
		
		if (cosTheta < -0.01) bgColor = data0.rgb;
		color = vec4(ssr(cameraSpacePosition, reflectDir, bgColor, cosTheta < -0.01), 1.0);
		
		float albedo = 1.0 - cosTheta;
		if (blockID == WaterID) albedo *= 1.0;
		else if (blockID == IceID) albedo *= 0.2;
		else if (blockID == IronID) albedo *= 0.4;
		
		color = vec4(mix(data0.rgb, color.rgb, clamp(albedo, 0.0, 1.0)), 1.0);
	} else {
		color = vec4(data0.rgb, 1.0);
	}
	
	// Fog
	color = vec4(mix(getSkyColor(viewDir).rgb, color.rgb, clamp((RenderDistance - dist) / 32.0, 0.0, 1.0)), color.a);
	
#ifdef VOLUMETRIC_CLOUDS
	vec4 cloudColor = cloud(vec3(PlayerPositionMod) + PlayerPositionFrac, viewDir, CloudStep, min(RenderDistance * CloudRangeMult, dist));
	color = vec4(cloudColor.rgb + (1.0 - cloudColor.a) * color.rgb, 1.0);
#endif
	
	gl_FragColor = color;
	gl_FragDepth = screenSpacePosition.z * 0.5 + 0.5;
}

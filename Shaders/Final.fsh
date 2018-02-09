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
const int MaxIterations = 50, MaxNearIterations = 12;
const int CloudIterations = 64, WaterCloudIterations = 2;
const float stepScale = 4.0;

const int RepeatLength = 1024;
const vec3 CloudScale = vec3(100.0, 50.0, 100.0);

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

vec3 getTextureNormal(in vec2 texCoord) {
	return normalize((ModelViewMatrix * vec4(texture2D(Texture1, texCoord).rgb * 2.0 - vec3(1.0), 0.0)).xyz);
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
		vec4(0.75, 0.95, 0.9, 1.0),
		vec4(152.0 / 255.0, 211.0 / 255.0, 250.0 / 255.0, 1.0),
		//vec4(152.0 / 255.0, 211.0 / 255.0, 250.0 / 255.0, 1.0),
		//vec4(90.0 / 255.0, 134.0 / 255.0, 206.0 / 255.0, 1.0),
		smoothstep(0.0, 1.0, normalize(dir).y * 2.0)
	);
}

vec3 ssr(in vec3 org, in vec3 dir, in vec3 bgColor, bool underWater) {
	vec3 curr = org, last = org;
	bool found = false;
	float ratio = 1.0, step = clamp(getDist(org) / 10.0, 0.1, 4.0);
	
	dir = normalize(dir);
	
	curr += dir * step * stepScale;
	
	for (int i = 0; i < MaxIterations; i++) {
		vec2 texCoord = cameraSpaceToTextureCoords(curr).xy;
		if (!inside(texCoord)) break;
		
		float currDist = getDist(curr);
		float pixelDist = getDist(getTexturePosition(texCoord));
		
		if (currDist < 0.0 || currDist > RenderDistance) break;
		
		if (getBlockID(texCoord) != 0 && getBlockID(texCoord) != WaterID &&
				pixelDist < currDist && (currDist - pixelDist < step * stepScale * 2.0 + 1.0 || underWater)) {
			found = true;
			dir /= 2.0;
			curr = last;
			ratio = min(ratio, float(i) / float(MaxIterations));
		} else last = curr;
		
		if (i >= MaxNearIterations) step = max(step, 1.0);
		
		curr += dir * step * stepScale;
		
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

float random(in float f) {
	return fract(sin(f * 233.0) * 43758.5453);
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
	res += interpolatedNoise3D(c * 1.0) * 1.0;
	res += interpolatedNoise3D(c * 2.0) * 0.5;
	res += interpolatedNoise3D(c * 4.0) * 0.25;
	res += interpolatedNoise3D(c * 8.0) * 0.125;
	//return clamp(res / 2.0 * 3.0 - 1.0, 0.0, 1.0);
	res = clamp(abs(res / 2.0 * 2.0 - 1.0) * 2.0 - 0.2, 0.0, 1.0);
	return res;// < 0.2 ? 0.0 : res;
}

vec4 cloud(in vec3 org, in vec3 dir, in float maxDist, in int iterations) {
	const float Bottom = 160.0, Top = 240.0, Transition = 40.0;
	
	vec3 curr = org;
	vec4 res = vec4(0.0);
	
	dir = normalize(dir);
	
	// Adjust step length according to iterations
	float step = 8.0;//(Top - Bottom) / float(iterations) * 2.0;
	// Adjust opacity factor according to iterations
	float alpha = 0.5;//1.0 - pow(0.003, 1.0 / float(iterations));
	
	// Skip non-cloud area
	if (curr.y < Bottom) {
		if (dir.y <= 0.0) return res;
		curr += dir * (Bottom - curr.y) / dir.y;
	} else if (curr.y > Top) {
		if (dir.y >= 0.0) return res;
		curr += dir * (Top - curr.y) / dir.y;
	}
	
	for (int i = 0; i < 16; i++) {
		curr += dir * step;
		
		if (curr.y >= Bottom && curr.y <= Top) {
			float factor = min(smoothstep(Bottom, Bottom + Transition, curr.y), 1.0 - smoothstep(Top - Transition, Top, curr.y));
			float opacity = factor * 0.5 * perlinNoise3D(curr / CloudScale);
			float diff = opacity - factor * 0.5 * perlinNoise3D(curr / CloudScale - SunlightDirection * 0.2);
			float col = 0.8 + diff;
			col = clamp(col, 0.2, 1.0);
			res.rgb = vec3(col) * (1.0 - res.a) + res.rgb * res.a;
			res.a = 1.0 - (1.0 - res.a) * (1.0 - opacity);
		} else return res;
		
		step *= 1.02;
		
		if (res.a > 0.99) return res;
		if (length(curr - org) > maxDist) return res;
	}
	
	return res;
}

float edgeDetect(in vec2 texCoord) {
	const int Samples = 16;
	const float Radius = 1.0;
	float res = 0.0;
	for (int i = 0; i < Samples; i++) {
		vec2 sp = texCoord + vec2(random(float(i)) * 2.0 - 1.0, random(float(i) + 0.5) * 2.0 - 1.0) * Radius / BufferSize;
		vec3 normal = getTextureNormal(texCoord);
		float normalDiff = -dot(normal, getTextureNormal(sp));
		normal = normalize((ProjectionMatrix * vec4(normal, 0.0)).xyz);
		float dist = length(getTexturePosition(texCoord));
		float distDiff = abs(dist - length(getTexturePosition(sp)));
		//float depthDiff = abs(getTextureDepth(texCoord) - getTextureDepth(sp));
		if (normalDiff > -0.9 || distDiff > max(0.9, dist * 0.01)) res += 1.0;
	}
	return res / float(Samples) * 2.0;
}
float sigmoid(float x)
{
	return 1.0/(1.0+exp(-1.0*x));
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
		vec3 normal = getTextureNormal(texCoord);
		vec3 reflectDir = reflect(normalize(cameraSpacePosition), normal);
		float cosTheta = dot(normalize(-cameraSpacePosition), normal);
		
		vec3 worldReflectDir = (ModelViewInverse * vec4(reflectDir, 0.0)).xyz;
#ifdef VOLUMETRIC_CLOUDS
		vec3 worldPosition = vec3(PlayerPositionInt) + PlayerPositionFrac + divide(ModelViewInverse * vec4(cameraSpacePosition, 1.0));
		vec4 cloud = cloud(worldPosition, worldReflectDir, RenderDistance * 16.0, WaterCloudIterations);
		vec3 bgColor = mix(getSkyColor(worldReflectDir).rgb, cloud.rgb, cloud.a);
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
	
#ifdef VOLUMETRIC_CLOUDS
	vec4 cloud = cloud(vec3(PlayerPositionMod) + PlayerPositionFrac, viewDir, dist, CloudIterations);
	color = vec4(mix(color.rgb, cloud.rgb, cloud.a), 1.0);
#endif
	
	float gs = 0.2989 * color.r + 0.5870 * color.g + 0.1140 * color.b;//(color.r + color.g + color.b) / 3.0;
	gs = min(gs, 1.0 - edgeDetect(texCoord));
	//gs = smoothstep(0.2, 0.8, gs);
	gs = sigmoid(gs*3.0-1.5);
	gs = floor(gs * 10.0) / 10.0;
	if(gs>0.4)gs=1.0;
	//else if(gs>0.45)gs=0.7;
	else gs=0.0;
	//if (random(texCoord.x * 233.0 + texCoord.y * 666.0) > gs) gs = 0.0; else gs = 1.0;
	
	color = vec4(vec3(gs,gs,gs), color.a);
	
	gl_FragColor = color;//vec4(mix(getSkyColor(viewDir).rgb, color.rgb, clamp((RenderDistance - dist) / 32.0, 0.0, 1.0)), color.a);
	
	float finalDepth = screenSpacePosition.z;
	if (blockID == 0) finalDepth = 1.0;
	
	gl_FragDepth = finalDepth * 0.5 + 0.5;
}

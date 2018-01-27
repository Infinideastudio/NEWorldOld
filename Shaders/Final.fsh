#version 110

uniform sampler2D Texture0;
uniform sampler2D Texture1;
uniform sampler2D Texture2;
uniform sampler2D Texture3;
uniform sampler2D Texture4;
uniform sampler2D Texture5;
uniform float ScreenWidth;
uniform float ScreenHeight;
uniform float BufferSize;
uniform mat4 ProjectionMatrix;
uniform mat4 ModelViewMatrix;
uniform float RenderDistance;
uniform vec4 BackgroundColor;

const float MaxBlockID = 4096.0;
const int GlassID = 9, WaterID = 10, IceID = 15, IronID = 17;
const int MaxIterations = 200, MaxNearIterations = 20;

float xScale, yScale, xRange, yRange, blockIDf;
int blockID;
vec3 cameraSpacePosition;

float decodeFloat16(in vec2 v) {
	return dot(v, vec2(1.0, 1.0 / 255.0));
}

float decodeFloat24(in vec3 v) {
	return dot(v, vec3(1.0, 1.0 / 255.0, 1.0 / 255.0 / 255.0));
}

vec3 cameraSpaceToTextureCoords(in vec3 v) {
	vec4 v4 = ProjectionMatrix * vec4(v, 1.0);
	v4 /= v4.w;
	vec3 v3 = v4.xyz / 2.0 + vec3(0.5, 0.5, 0.5);
	v3.x *= xRange;
	v3.y *= yRange;
	return v3;
}

vec3 getTexturePosition(in vec2 texCoord) {
	vec4 data3 = texture2D(Texture3, texCoord);
	vec4 data4 = texture2D(Texture4, texCoord);
	vec4 data5 = texture2D(Texture5, texCoord);
	
	float xpos = decodeFloat24(data3.rgb);
	float ypos = decodeFloat24(data4.rgb);
	float zpos = decodeFloat24(data5.rgb);
	
	return vec3(xpos, ypos, zpos) * 2.0 * RenderDistance - vec3(RenderDistance, RenderDistance, RenderDistance);
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

vec3 search(in vec3 org, in vec3 dir, in vec3 bgColor) {
	vec3 curr = org, last = org;
	bool found = false;
	float ratio = 1.0, step = clamp(getDist(org) / 10.0, 0.1, 4.0);
	
	dir = normalize(dir);
	
	curr += dir;
	
	for (int i = 0; i < MaxIterations; i++) {
		vec2 texCoord = cameraSpaceToTextureCoords(curr).xy;
		if (!inside(texCoord)) break;
		
		float currDist = getDist(curr);
		float pixelDist = getDist(getTexturePosition(texCoord));
		
		if (currDist < 0.0 || currDist > RenderDistance) break;
		
		if (getBlockID(texCoord) != 0 && getBlockID(texCoord) != WaterID && pixelDist < currDist) {
			found = true;
			dir /= 2.0;
			curr = last;
			ratio = min(ratio, float(i) / float(MaxIterations));
		} else last = curr;
		
		curr += dir * step;
		
		if (i > MaxNearIterations) step = max(step, 1.0);
		
		if (found && (length(dir) < 0.1 || i == MaxIterations - 1)) {
			float factor = clamp(min(distToEdge(texCoord) * 5.0, (1.0 - ratio) * 5.0), 0.0, 1.0);
			//float factor = clamp(distToEdge(texCoord) * 5.0, 0.0, 1.0);
			return mix(bgColor, texture2D(Texture0, texCoord).rgb, factor);
		}
	}
	
	return bgColor;
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
	
	vec4 color;
	
	cameraSpacePosition = getTexturePosition(texCoord);
	float dist = getDist(cameraSpacePosition);
	
	if (blockID == WaterID || blockID == IceID || blockID == IronID) {
		vec3 normal = (ModelViewMatrix * vec4(data1.rgb * 2.0 - vec3(1.0, 1.0, 1.0), 0.0)).xyz;
		vec3 reflectDir = reflect(normalize(cameraSpacePosition), normalize(normal));
		float cosTheta = dot(normalize(-cameraSpacePosition), normalize(normal));
		
		vec3 bgColor = BackgroundColor.rgb;
		if (cosTheta < -0.01) bgColor = data0.rgb;
		color = vec4(search(cameraSpacePosition, reflectDir, bgColor), 1.0);
		
		if (blockID == WaterID) cosTheta += 0.0;
		else if (blockID == IceID) cosTheta += 0.6;
		else if (blockID == IronID) cosTheta += 0.4;
		
		color = vec4(mix(color.rgb, data0.rgb, clamp(cosTheta, 0.0, 1.0)), 1.0);
	} else {
		color = vec4(data0.rgb, 1.0);
	}
	
	gl_FragColor = color;
}

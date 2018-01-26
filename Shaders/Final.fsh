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
uniform mat4 NormalMatrix;
uniform float RenderDistance;
uniform vec4 BackgroundColor;

const float MaxBlockID = 4096.0;
const int WaterID = 10;
const int MaxIterations = 200;

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
	return min(min(v.x, 1.0 - v.x), min(v.y, 1.0 - v.y) / yRange);
}

vec3 search(in vec3 org, in vec3 dir) {
	vec3 curr = org, last = org;
	bool found = false;
	
	dir = normalize(dir);// * getDist(org) / 5.0;
	
	curr += dir;
	
	for (int i = 0; i < MaxIterations; i++) {
		
		vec2 texCoord = cameraSpaceToTextureCoords(curr).xy;
		
		if (!inside(texCoord)) break;
		
		float pixelDist = getDist(getTexturePosition(texCoord));
		
		if (getBlockID(texCoord) != 0 && pixelDist < getDist(curr)) {
			found = true;
			dir /= 2.0;
			curr = last;
		} else last = curr;
		
		curr += dir;
		
		if (found && (length(dir) < 0.02 || i == MaxIterations - 1)) {
			float factor = clamp(distToEdge(texCoord) * 5.0, 0.0, 1.0);
			return mix(BackgroundColor.rgb, texture2D(Texture0, texCoord).rgb, factor);
		}
	}
	
	return BackgroundColor.rgb;
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
	
	if (blockID == WaterID) {
		
		vec3 normal = (NormalMatrix * vec4(data1.rgb * 2.0 - vec3(1.0, 1.0, 1.0), 0.0)).xyz;
		
		//if (normal.y > 0.1) normal = getWavesNormal();
		
		float cosTheta = dot(normalize(-cameraSpacePosition), normalize(normal));
		
		vec3 reflectDir = reflect(normalize(cameraSpacePosition), normalize(normal));
		
		color = vec4(search(cameraSpacePosition, reflectDir), data0.a);
		
		color = vec4(mix(color.rgb, data0.rgb, clamp(cosTheta, 0.0, 1.0)), 1.0);
		
	} else {
		color = vec4(data0.rgb, 1.0);
	}
	
	gl_FragColor = color;
}

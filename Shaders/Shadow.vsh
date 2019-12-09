#version 110

uniform float GameTime;
attribute float VertexAttrib;

const float Pi = 3.1415926;
const int LeafID = 8;

vec2 FisheyeProjection(vec2 position) {
	const float FisheyeFactor = 0.85;
	float dist = length(position);
    float distortFactor = (1.0 - FisheyeFactor) + dist * FisheyeFactor;
    position /= distortFactor;
	return position;
}

void main() {
	vec4 vertex = gl_Vertex;
	float blockIDf = VertexAttrib;
	
	int blockID = int(blockIDf + 0.5);
	if (blockID == LeafID) {
		float a = GameTime * 0.2;
		vertex += vec4(sin(vertex.x + a), sin(vertex.y * 10.0 + a + Pi / 3.0 * 2.0), sin(vertex.z * 10.0 + a + Pi / 3.0 * 4.0), 0.0) * 0.005;
	}
	
	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vertex;
	gl_Position = vec4(FisheyeProjection(gl_Position.xy), gl_Position.zw);
}

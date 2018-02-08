#version 110

uniform mat4 Translation;
uniform float RenderDistance;
uniform float GameTime;

attribute float VertexAttrib;
varying vec4 vertCoords;
varying float blockIDf;

varying vec3 normal;
varying vec4 cameraSpacePosition;
varying vec4 screenSpacePosition;

const float Pi = 3.1415926;
const int LeafID = 8;

float radius = RenderDistance * 2.0;

void main() {
    // Spherical look
	/*
    vec4 realPosition = Translation * gl_Vertex;
    realPosition /= realPosition.w;
    float height = realPosition.y;
    vec3 relativePosition = realPosition.xyz + vec3(0.0, radius, 0.0);
    relativePosition /= length(relativePosition);
    relativePosition *= (radius + height);
    vec4 offset = vec4(relativePosition - vec3(0.0, radius, 0.0) - realPosition.xyz, 0.0);
	*/
	
	vec4 vertex = gl_Vertex;
	blockIDf = VertexAttrib;
	
	int blockID = int(blockIDf + 0.5);
	if (blockID == LeafID) {
		float a = GameTime * 0.2;
		vertex += vec4(sin(vertex.x + a), sin(vertex.y * 10.0 + a + Pi / 3.0 * 2.0), sin(vertex.z * 10.0 + a + Pi / 3.0 * 4.0), 0.0) * 0.005;
	}
	
    // Results
	vertCoords = vertex;
	normal = gl_Normal;
	gl_FrontColor = gl_Color;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	cameraSpacePosition = gl_ModelViewMatrix * vertex;
	screenSpacePosition = gl_ProjectionMatrix * gl_ModelViewMatrix * vertex;
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * vertex;//((gl_Vertex + offset) / gl_Vertex.w);
}

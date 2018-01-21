#version 110

uniform mat4 Translation;
uniform float RenderDistance;

attribute float VertexAttrib;
varying vec4 vertCoords;
varying float facingFloat;

float radius = RenderDistance * 2.0;

void main() {
    // Spherical look
    vec4 realPosition = Translation * gl_Vertex;
    realPosition /= realPosition.w;
    float height = realPosition.y;
    vec3 relativePosition = realPosition.xyz + vec3(0.0, radius, 0.0);
    relativePosition /= length(relativePosition);
    relativePosition *= (radius + height);
    vec4 offset = vec4(relativePosition - vec3(0.0, radius, 0.0) - realPosition.xyz, 0.0);
    // Results
	facingFloat = VertexAttrib;
	vertCoords = gl_Vertex;
	gl_FrontColor = gl_Color;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * ((gl_Vertex + offset) / gl_Vertex.w);
}

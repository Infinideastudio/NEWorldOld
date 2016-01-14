#version 110
uniform int renderdist;
varying float dist;
void main() {
	vec4 rel = gl_ModelViewMatrix * gl_Vertex;
	dist = sqrt(rel.x * rel.x + rel.y * rel.y + rel.z * rel.z);
	gl_FrontColor = gl_Color;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_Position = gl_ProjectionMatrix * rel;
}
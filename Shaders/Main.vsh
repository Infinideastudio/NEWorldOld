#version 110
uniform float renderdist;
varying float dist;
void main() {
	vec4 rel = gl_ModelViewMatrix * gl_Vertex;
	dist = length(rel);
	gl_FrontColor = gl_Color;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	/*
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix *
		vec4(gl_Vertex.x, gl_Vertex.y +
		(cos(rel.x / renderdist) - 1.0) * renderdist / 4.0 +
		(cos(rel.z / renderdist) - 1.0) * renderdist / 4.0,
		gl_Vertex.zw);
	*/
	gl_Position = gl_ProjectionMatrix * rel;
}
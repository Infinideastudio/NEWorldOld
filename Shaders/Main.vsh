#version 110
uniform mat4 Depth_proj;
uniform mat4 Depth_modl;
varying float dist;
varying vec4 position;
void main() {
	position = Depth_proj * Depth_modl * gl_Vertex;
	position /= position.w;
	position.x = (position.x + 1.0) / 2.0;
	position.y = (position.y + 1.0) / 2.0;
	//position.z -= 0.001;
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
#version 110
uniform mat4 Depth_proj;
uniform mat4 Depth_modl;
uniform mat4 TransMat;
uniform float renderdist;
varying float dist;
varying vec4 position;
void main() {
	position = Depth_proj * Depth_modl * TransMat * gl_Vertex;
	//Normalize position
	position /= position.w;
	position.xyz = (position.xyz + vec3(1.0, 1.0, 1.0)) / 2.0;
	position.z -= 0.001;
	//Distance for fog calculation
	vec4 rel = gl_ModelViewMatrix * gl_Vertex;
	dist = length(rel);
	//Color & Texture coords
	gl_FrontColor = gl_Color;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	//Final position
	gl_Position = gl_ProjectionMatrix * rel;
}
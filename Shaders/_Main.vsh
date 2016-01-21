#version 110

#define SMOOTH_SHADOW

const mat4 normalize = mat4(
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 0.5, 0.0,
	0.5, 0.5, 0.499, 1.0);
const float delta = 0.05;

uniform mat4 Depth_proj;
uniform mat4 Depth_modl;
uniform mat4 TransMat;
uniform float renderdist;

//如果你搞不懂这俩变量是干啥的就去看Renderer.cpp吧。。。
attribute float VertexAttrib;
varying float facing_float;

varying float dist;
varying vec4 ShadowCoords;

#ifdef SMOOTH_SHADOW
//偏移的ShadowCoords用于平滑阴影边缘
//注：后面两个数字是坐标，这个坐标的原点为(1,1)。。。主要是-1不好写。。。
varying vec4 ShadowCoords01;
varying vec4 ShadowCoords21;
varying vec4 ShadowCoords10;
varying vec4 ShadowCoords12;
#endif

void main() {
	
	//Additional vertex attributes (面的朝向)
	facing_float = VertexAttrib;
	int facing = int(facing_float + 0.5);
	
	//Distance for fog calculation
	vec4 rel = gl_ModelViewMatrix * gl_Vertex;
	dist = length(rel);
	
	//Color & Texture coords
	gl_FrontColor = gl_Color;
	gl_TexCoord[0] = gl_MultiTexCoord0;
	
	/*
	//Sphere shaped world & Final position
	gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix *
		vec4(gl_Vertex.x, gl_Vertex.y +
		(cos(rel.x / renderdist) - 1.0) * renderdist / 4.0 +
		(cos(rel.z / renderdist) - 1.0) * renderdist / 4.0,
		gl_Vertex.zw);
	*/
	
	//Final position
	gl_Position = gl_ProjectionMatrix * rel;
	
	//Calculate transform matrix for shadow coords
	mat4 transf = normalize * Depth_proj * Depth_modl * TransMat;
	
	//Calculate shadow coords
	ShadowCoords = transf * gl_Vertex;
#ifdef SMOOTH_SHADOW
	//Shadow smoothing (Super-sample)
	if (dist < 20.0) {
		if (facing == 0 || facing == 1) {
			ShadowCoords01 = transf * vec4(gl_Vertex.x - delta, gl_Vertex.yzw);
			ShadowCoords21 = transf * vec4(gl_Vertex.x + delta, gl_Vertex.yzw);
			ShadowCoords10 = transf * vec4(gl_Vertex.x, gl_Vertex.y - delta, gl_Vertex.zw);
			ShadowCoords12 = transf * vec4(gl_Vertex.x, gl_Vertex.y + delta, gl_Vertex.zw);
		}
		else if (facing == 2 || facing == 3) {
			ShadowCoords01 = transf * vec4(gl_Vertex.x, gl_Vertex.y - delta, gl_Vertex.zw);
			ShadowCoords21 = transf * vec4(gl_Vertex.x, gl_Vertex.y + delta, gl_Vertex.zw);
			ShadowCoords10 = transf * vec4(gl_Vertex.xy, gl_Vertex.z - delta, gl_Vertex.w);
			ShadowCoords12 = transf * vec4(gl_Vertex.xy, gl_Vertex.z + delta, gl_Vertex.w);
		}
		else if (facing == 4 || facing == 5) {
			ShadowCoords01 = transf * vec4(gl_Vertex.x - delta, gl_Vertex.yzw);
			ShadowCoords21 = transf * vec4(gl_Vertex.x + delta, gl_Vertex.yzw);
			ShadowCoords10 = transf * vec4(gl_Vertex.xy, gl_Vertex.z - delta, gl_Vertex.w);
			ShadowCoords12 = transf * vec4(gl_Vertex.xy, gl_Vertex.z + delta, gl_Vertex.w);
		}
	}
#endif
}
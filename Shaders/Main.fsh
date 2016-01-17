#version 110
uniform sampler2D Tex;
uniform sampler2D DepthTex;
uniform float renderdist;
varying float dist;
varying vec4 position;
void main() {
    //float delta = 1.0 / 2048.0;
	//float ldepth = texture2D(DepthTex, position.xy).z;
    //ldepth += texture2D(DepthTex, position.xy + vec2(-delta, -delta)).z;
    //ldepth += texture2D(DepthTex, position.xy + vec2( delta, -delta)).z;
    //ldepth += texture2D(DepthTex, position.xy + vec2(-delta,  delta)).z;
    //ldepth += texture2D(DepthTex, position.xy + vec2( delta,  delta)).z;
    //ldepth *= 0.2;
	float shadow = (position.z < texture2D(DepthTex, position.xy).z) ? 1.0 : 0.5;
	//shadow += (position.z < texture2D(DepthTex, position.xy + vec2(-delta, -delta)).z) ? 1.0 : 0.2;
	//shadow += (position.z < texture2D(DepthTex, position.xy + vec2( delta, -delta)).z) ? 1.0 : 0.2;
	//shadow += (position.z < texture2D(DepthTex, position.xy + vec2(-delta,  delta)).z) ? 1.0 : 0.2;
	//shadow += (position.z < texture2D(DepthTex, position.xy + vec2( delta,  delta)).z) ? 1.0 : 0.2;
	//shadow *= 0.2;
	vec4 color = texture2D(Tex, gl_TexCoord[0].st) * shadow * gl_Color;
	//Fog calculation & Final color
	gl_FragColor = mix(vec4(0.7, 1.0, 1.0, 1.0), color, clamp((renderdist - dist) / 32.0, 0.0, 1.0));
}
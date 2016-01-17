#version 110
uniform sampler2D Tex;
uniform sampler2D DepthTex;
uniform float renderdist;
varying float dist;
varying vec4 position;
void main() {
    float delta = 1.0 / 8192.0;
	float ldepth = 0.0;
    ldepth += texture2D(DepthTex, position.xy + vec2(-delta, -delta)).z;
    ldepth += texture2D(DepthTex, position.xy + vec2( delta, -delta)).z;
    ldepth += texture2D(DepthTex, position.xy + vec2(-delta,  delta)).z;
    ldepth += texture2D(DepthTex, position.xy + vec2( delta,  delta)).z;
    ldepth *= 0.25;
	float shadow = (position.z - 0.01 < ldepth) ? 1.0 : 0.5;
	vec4 color = vec4(texture2D(Tex, gl_TexCoord[0].st).rgb * shadow, 1.0) * gl_Color;
	//Fog calculation & Final color
	gl_FragColor = mix(vec4(0.7, 1.0, 1.0, 1.0), color, clamp((renderdist - dist) / 32.0, 0.0, 1.0));
	//gl_FragColor = vec4(position.zzz, 1.0);
}
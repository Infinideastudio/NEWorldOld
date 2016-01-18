#version 110
uniform sampler2D Tex;
uniform sampler2D DepthTex;
uniform float renderdist;
varying float dist;
varying vec4 position;
void main() {
    //float delta = 1.0 / 8192.0;
	//float ldepth = texture2D(DepthTex, position.xy).z;
    //ldepth += texture2D(DepthTex, position.xy + vec2(-delta, -delta)).z;
    //ldepth += texture2D(DepthTex, position.xy + vec2( delta, -delta)).z;
    //ldepth += texture2D(DepthTex, position.xy + vec2(-delta,  delta)).z;
    //ldepth += texture2D(DepthTex, position.xy + vec2( delta,  delta)).z;
    //ldepth *= 0.2;
	float shadow = (position.z < texture2D(DepthTex, position.xy).z ||
		position.x < 0.0 || position.x > 1.0 || position.y < 0.0 || position.y > 1.0 || position.z > 1.0) ? 1.2 : 0.5;
	//float shadow = (position.z < texture2D(DepthTex, position.xy).z) ? 1.2 : 0.5;
	vec4 texel = texture2D(Tex, gl_TexCoord[0].st);
	vec4 color = vec4(texel.rgb * shadow, texel.a) * gl_Color;
	//Fog calculation & Final color
	gl_FragColor = mix(vec4(0.7, 1.0, 1.0, 1.0), color, clamp((renderdist - dist) / 32.0, 0.0, 1.0));
}
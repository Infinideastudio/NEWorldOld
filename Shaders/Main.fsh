#version 110
uniform sampler2D Tex;
uniform sampler2D DepthTex;
uniform float renderdist;
varying float dist;
varying vec4 position;
void main() {
	float ldepth = texture2D(DepthTex, position.xy).z;
	float shadow = (position.z <= ldepth) ? 1.0 : 0.0;
	vec4 color = mix(texture2D(Tex, gl_TexCoord[0].st), vec4(shadow, shadow, shadow, 1.0), 0.5) * gl_Color;
	gl_FragColor = mix(vec4(0.7, 1.0, 1.0, 1.0), color, clamp((renderdist - dist) / 32.0, 0.0, 1.0));
}
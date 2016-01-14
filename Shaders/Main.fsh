#version 110
uniform sampler2D tex;
uniform float renderdist;
varying float dist;
void main() {
	gl_FragColor = mix(vec4(0.7, 1.0, 1.0, 1.0),
		texture2D(tex,gl_TexCoord[0].st) * gl_Color,
		clamp((renderdist - dist) / 32.0, 0.0, 1.0));
}
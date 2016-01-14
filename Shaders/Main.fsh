#version 110
uniform sampler2D tex;
uniform int renderdist;
varying float dist;
void main() {
	vec4 color = texture2D(tex,gl_TexCoord[0].st) * gl_Color;
	if (dist < float(renderdist) * 16.0 - 32.0) gl_FragColor = color;
	else if (dist > float(renderdist) * 16.0) gl_FragColor = vec4(0.7, 1.0, 1.0, 1.0);
	else gl_FragColor = mix(vec4(0.7, 1.0, 1.0, 1.0), color, (float(renderdist) * 16.0 - dist) / 32.0);
}
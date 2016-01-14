#version 110
uniform sampler2D tex;
uniform float texwidth;
void main() {
	vec2 pos = vec2(gl_TexCoord[0].s - floor(gl_TexCoord[0].s / texwidth) * texwidth, gl_TexCoord[0].t);
	vec4 color = texture2D(tex, pos);
	gl_FragColor = color * gl_Color;
}
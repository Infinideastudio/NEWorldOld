#version 110
uniform sampler2D Tex;
void main() {
	float color = texture2D(Tex, gl_TexCoord[0].xy).z;
	gl_FragColor = vec4(color, color, color, 1.0);
}
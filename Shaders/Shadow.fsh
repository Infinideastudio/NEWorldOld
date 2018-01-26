#version 110

uniform sampler2D Texture;

void main() {
	vec4 texel = texture2D(Texture, gl_TexCoord[0].st);
	gl_FragColor = texel;
}

#version 110

uniform sampler2DShadow Tex;

float sampleShadow(in vec2 pos) {
	float first = 0.0, last = 1.0, mid;
	for (int i = 0; i < 8; i++) {
		mid = (first + last) / 2.0;
		if (shadow2D(Tex, vec3(pos, mid)).z > 0.5) first = mid; else last = mid;
	}
	return mid;
}

void main() {
	float texel = sampleShadow(gl_TexCoord[0].st);
	if (texel > 254.0 / 255.0) {
		gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
		return;
	}
	gl_FragColor = vec4(texel, texel, texel, 1.0);
}

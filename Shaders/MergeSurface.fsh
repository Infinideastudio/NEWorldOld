#version 110
uniform sampler3D Tex;
uniform sampler2D DepthTex;
uniform float renderdist;
varying float dist;
varying vec4 position;
void main() {
	float shadow = (position.z < texture2D(DepthTex, position.xy).z ||
		position.x < 0.0 || position.x > 1.0 || position.y < 0.0 || position.y > 1.0 || position.z > 1.0) ? 1.2 : 0.5;
	vec4 texel = texture3D(Tex, gl_TexCoord[0].stp);
	vec4 color = vec4(texel.rgb * shadow, texel.a) * gl_Color;
	//Fog calculation & Final color
	gl_FragColor = mix(vec4(0.7, 1.0, 1.0, 1.0), color, clamp((renderdist - dist) / 32.0, 0.0, 1.0));
}
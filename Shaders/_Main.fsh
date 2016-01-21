#version 110

#define SMOOTH_SHADOW

uniform sampler2D Tex;
uniform sampler2D DepthTex;
uniform float renderdist;
varying float facing_float;
varying float dist;

varying vec4 ShadowCoords;
#ifdef SMOOTH_SHADOW
varying vec4 ShadowCoords01;
varying vec4 ShadowCoords21;
varying vec4 ShadowCoords10;
varying vec4 ShadowCoords12;
#endif

void main() {
	//Shadow color
	float shadow = 0.0;
	int facing = int(facing_float + 0.5);
	//Shadow calculation
	if (facing == 1 || facing == 2 || facing == 5) shadow = 0.5;
	else if (ShadowCoords.x >= 0.0 && ShadowCoords.x <= 1.0 &&
		ShadowCoords.y >= 0.0 && ShadowCoords.y <= 1.0 && ShadowCoords.z <= 1.0) {
		if (ShadowCoords.z < texture2D(DepthTex, ShadowCoords.xy).z) shadow += 1.2; else shadow += 0.5;
#ifdef SMOOTH_SHADOW
		if (dist < 16.0) {
			if (ShadowCoords01.z < texture2D(DepthTex, ShadowCoords01.xy).z) shadow += 1.2; else shadow += 0.5;
			if (ShadowCoords21.z < texture2D(DepthTex, ShadowCoords21.xy).z) shadow += 1.2; else shadow += 0.5;
			if (ShadowCoords10.z < texture2D(DepthTex, ShadowCoords10.xy).z) shadow += 1.2; else shadow += 0.5;
			if (ShadowCoords12.z < texture2D(DepthTex, ShadowCoords12.xy).z) shadow += 1.2; else shadow += 0.5;
			shadow *= 0.2;
		}
#endif
	}
	else shadow = 1.2;
	
	//Texture color
	vec4 texel = texture2D(Tex, gl_TexCoord[0].st);
	vec4 color = vec4(texel.rgb * shadow, texel.a) * gl_Color;
	
	//Fog calculation & Final color
	gl_FragColor = mix(vec4(0.7, 1.0, 1.0, 1.0), color, clamp((renderdist - dist) / 32.0, 0.0, 1.0));
}
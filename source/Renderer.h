#pragma once
#include "Definitions.h"
#include "GLProc.h"
#include "Frustum.h"

namespace Renderer{
	//我猜你肯定不敢看Renderer.cpp  --qiaozhanrong

	const int ArraySize = 2621440;
	extern float* VA;
	extern int size;
	extern int Vertexes;
	extern bool AdvancedRender;
	extern int ShadowRes;
	extern int MaxShadowDist;
	extern int shadowdist;
	extern float sunlightXrot, sunlightYrot;
	extern unsigned int DepthTexture;
	extern GLhandleARB shaders[16];
	extern GLhandleARB shaderPrograms[16];
	extern int ActiveShader;

	void Init(int tc, int cc, int ac = 0);
	void Vertex3f(float x, float y, float z);
	void TexCoord2f(float x, float y);
	void TexCoord3f(float x, float y, float z);
	void Color3f(float r, float g, float b);
	void Color4f(float r, float g, float b, float a);
	void Attrib1f(float attr);

	inline void Vertex3d(double x, double y, double z) { Vertex3f((float)x, (float)y, (float)z); }
	inline void TexCoord2d(double x, double y) { TexCoord2f((float)x, (float)y); }
	inline void TexCoord3d(double x, double y, double z) { TexCoord3f((float)x, (float)y, (float)z); }
	inline void Color3d(double r, double g, double b) { Color3f((float)r, (float)g, (float)b); }
	inline void Color4d(double r, double g, double b, double a) { Color4f((float)r, (float)g, (float)b, (float)a); }

	inline void Quad(float *geomentry) {
		//这样做貌似提升不了性能吧。。。 --qiaozhanrong
		memcpy(VA, geomentry, size*sizeof(float)); VA += size;
		Vertexes += 4;
	}

	void Flush(VBOID& buffer, vtxCount& vtxs);
	void renderbuffer(VBOID buffer, vtxCount vtxs, int tc, int cc, int ac = 0);

	void initShaders();
	void destroyShaders();
	GLhandleARB loadShader(string filename, unsigned int mode);
	void printInfoLog(GLhandleARB obj);
	void EnableShaders();
	void DisableShaders();
	void StartShadowPass();
	void EndShadowPass();
}

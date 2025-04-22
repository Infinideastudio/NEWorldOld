#pragma once
#include "Definitions.h"
#include "GLProc.h"
#include "FrustumTest.h"
#include "Shader.h"
#include "Frustum.h"
#include "FrameBuffer.h"

namespace Renderer {
	//�Ҳ���϶����ҿ�Renderer.cpp  --qiaozhanrong
	//�¶���  --Null

	enum {
		MainShader, FinalShader, ShadowShader, ShowDepthShader
	};

	const int ArraySize = 4194304;
	extern float* VA;
	extern int size;
	extern int Vertexes;
	extern bool AdvancedRender;
	extern int ShadowRes;
	extern int MaxShadowDist;
	extern float sunlightXrot, sunlightYrot;
	extern Frustum playerFrustum;
	extern vector<Shader> shaders;
	extern int ActiveShader;

	extern FrameBuffer shadow;
	extern bool VolumetricClouds;

	void Init(int tc, int cc, int nc = 0, int ac = 0);
	void Vertex3f(float x, float y, float z);
	void TexCoord2f(float x, float y);
	void TexCoord3f(float x, float y, float z);
	void Color3f(float r, float g, float b);
	void Color4f(float r, float g, float b, float a);
	void Normal3f(float x, float y, float z);
	void Attrib1f(float attr);

	inline void Vertex3d(double x, double y, double z) { Vertex3f((float)x, (float)y, (float)z); }
	inline void TexCoord2d(double x, double y) { TexCoord2f((float)x, (float)y); }
	inline void TexCoord3d(double x, double y, double z) { TexCoord3f((float)x, (float)y, (float)z); }
	inline void Color3d(double r, double g, double b) { Color3f((float)r, (float)g, (float)b); }
	inline void Color4d(double r, double g, double b, double a) { Color4f((float)r, (float)g, (float)b, (float)a); }
	inline void Normal3d(double x, double y, double z) { Normal3f((float)x, (float)y, (float)z); }

	inline void Quad(float *geomentry) {
		//������ò�������������ܰɡ����� --qiaozhanrong
		memcpy(VA, geomentry, size * sizeof(float)); VA += size;
		Vertexes += 4;
	}

	void Flush(VBOID& buffer, vtxCount& vtxs);
	void renderbuffer(VBOID buffer, vtxCount vtxs, int tc, int cc, int nc = 0, int ac = 0);

	void initShaders();
	inline void bindShader(int shaderID) {
		shaders[shaderID].bind();
		ActiveShader = shaderID;
	}
	void destroyShaders();
	FrustumTest getLightFrustum();
	FrustumTest getShadowMapFrustum(double xpos, double ypos, double zpos, double heading, double pitch, int shadowdist, const FrustumTest& playerFrustum);
	
	void ClearBuffer();
	void StartMainPass(float gametime);
	void EndMainPass();
	
	void StartShadowPass();
	void EndShadowPass();

	void StartFinalPass(double xpos, double ypos, double zpos, double heading, double pitch, const FrustumTest& viewFrustum, float gameTime);
	void EndFinalPass();

	void DrawFullscreen();
}

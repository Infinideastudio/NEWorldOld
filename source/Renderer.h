#ifndef RENDERER_H
#define RENDERER_H
#include "Definitions.h"

namespace Renderer {
	const int ArrayUNITSIZE = 262144;
	extern float* VA;
	extern int size;
	extern int Vertexes;
	extern bool ShaderAval, UseShaders;

	void Init(int tcc, int cc);
	void Vertex3f(float x, float y, float z);
	void TexCoord2f(float x, float y);
	void TexCoord3f(float x, float y, float z);
	void Color3f(float r, float g, float b);
	void Color4f(float r, float g, float b, float a);

	inline void Vertex3d(double x, double y, double z) { Vertex3f((float)x, (float)y, (float)z); }
	inline void TexCoord2d(double x, double y) { TexCoord2f((float)x, (float)y); }
	inline void TexCoord3d(double x, double y, double z) { TexCoord3f((float)x, (float)y, (float)z); }
	inline void Color3d(double r, double g, double b) { Color3f((float)r, (float)g, (float)b); }
	inline void Color4d(double r, double g, double b, double a) { Color4f((float)r, (float)g, (float)b, (float)a); }
	inline void Quad(float *geomentry) {
		memcpy(VA, geomentry, size*sizeof(float)); VA += size;
		Vertexes += 4;
	}

	void Flush(VBOID& buffer, vtxCount& vtxs);
	void renderbuffer(VBOID buffer, vtxCount vtxs, int ctex, int ccol);

	void initShaders();
	void destroyShaders();
	GLhandleARB loadShader(string filename, unsigned int mode);
	void printInfoLog(GLhandleARB obj);
	void EnableShaders();
	void DisableShaders();
}
#endif
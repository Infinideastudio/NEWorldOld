#ifndef RENDERER_H
#define RENDERER_H
#include "Definitions.h"

namespace renderer{
	const int ArrayUNITSIZE = 65536;
	void Init();
	void Vertex3d(double x, double y, double z);
	void TexCoord2d(double x, double y);
	void Color3d(double r, double g, double b);
	void Vertex3f(float x, float y, float z);
	void TexCoord2f(float x, float y);
	void Color3f(float r, float g, float b);
	void Flush(VBOID& buffer, vtxCount& vtxs);
	void renderbuffer(VBOID buffer, vtxCount vtxs, bool ftex, bool fcol);
}
#endif
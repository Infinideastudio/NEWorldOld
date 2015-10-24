#pragma once
#include "Definitions.h"

namespace renderer{
	const int ArrayUNITSIZE = 65536;
	void Init();
	void Vertex3d(double x, double y, double z);
	void TexCoord2d(double x, double y);
	void Color3d(double r_, double g_, double b_);
	void Vertex3f(float x, float y, float z);
	void TexCoord2f(float x, float y);
	void Color3f(float r_, float g_, float b_);
	void Flush(unsigned int& buffer, int& vtxs);
	void renderbuffer(unsigned int buffer, int vtxs, bool ftex, bool fcol);
	inline void renderbuffer_opt(unsigned int buffer, int vtxs){
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
		glTexCoordPointer(2, GL_FLOAT, 8 * sizeof(float), (float*)0);
		glColorPointer(3, GL_FLOAT, 8 * sizeof(float), (float*)(2 * sizeof(float)));
		glVertexPointer(3, GL_FLOAT, 8 * sizeof(float), (float*)(5 * sizeof(float)));
		glDrawArrays(GL_QUADS, 0, vtxs);
	}
}

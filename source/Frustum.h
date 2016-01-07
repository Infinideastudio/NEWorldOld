#pragma once
#include "stdinclude.h"
#include "Hitbox.h"

namespace Frustum{
	extern float frus[24], clip[16];
	extern float proj[16], modl[16];
	void LoadIdentity();
	inline void MultMatrixTo(float* sum, float* a, float* b) {
		memset(sum, 0, sizeof(float*) * 16);
		for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
			for (int k = 0; k < 4; k++) sum[j * 4 + i] += a[k * 4 + i] * b[j * 4 + k];
		}
	}
	inline void MultMatrix(float* a, float* b) {
		float sum[16]; MultMatrixTo(sum, a, b);
		memcpy(a, sum, sizeof(float*) * 16);
	}
	void SetPerspective(float FOV, float aspect, float Znear, float Zfar);
	void MultRotate(float angle, float x, float y, float z);
	inline void normalize(int side) {
		float magnitude = (float)sqrt(frus[side + 0] * frus[side + 0] + frus[side + 1] * frus[side + 1] + frus[side + 2] * frus[side + 2]);
		frus[side + 0] /= magnitude; frus[side + 1] /= magnitude; frus[side + 2] /= magnitude; frus[side + 3] /= magnitude;
	}
	void update();
	bool FrustumTest(const Hitbox::AABB& aabb);
}


#ifndef FRUSTUM_H
#define FRUSTUM_H
#include "Definitions.h"
#include "Hitbox.h"

namespace Frustum{
	extern float frus[24], clip[16];
	extern float proj[16], modl[16];

	//AABB with Float32 coords
	struct ChunkBox {
		float xmin, ymin, zmin;
		float xmax, ymax, zmax;
	};

	void LoadIdentity();
	inline void MultMatrixTo(float* sum, float* a, float* b) {
        sum[0] = a[0] * b[0] + a[1] * b[4] + a[2] * b[8] + a[3] * b[12];
        sum[1] = a[0] * b[1] + a[1] * b[5] + a[2] * b[9] + a[3] * b[13];
        sum[2] = a[0] * b[2] + a[1] * b[6] + a[2] * b[10] + a[3] * b[14];
        sum[3] = a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + a[3] * b[15];
        sum[4] = a[4] * b[0] + a[5] * b[4] + a[6] * b[8] + a[7] * b[12];
        sum[5] = a[4] * b[1] + a[5] * b[5] + a[6] * b[9] + a[7] * b[13];
        sum[6] = a[4] * b[2] + a[5] * b[6] + a[6] * b[10] + a[7] * b[14];
        sum[7] = a[4] * b[3] + a[5] * b[7] + a[6] * b[11] + a[7] * b[15];
        sum[8] = a[8] * b[0] + a[9] * b[4] + a[10] * b[8] + a[11] * b[12];
        sum[9] = a[8] * b[1] + a[9] * b[5] + a[10] * b[9] + a[11] * b[13];
        sum[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10] + a[11] * b[14];
        sum[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11] * b[15];
        sum[12] = a[12] * b[0] + a[13] * b[4] + a[14] * b[8] + a[15] * b[12];
        sum[13] = a[12] * b[1] + a[13] * b[5] + a[14] * b[9] + a[15] * b[13];
        sum[14] = a[12] * b[2] + a[13] * b[6] + a[14] * b[10] + a[15] * b[14];
        sum[15] = a[12] * b[3] + a[13] * b[7] + a[14] * b[11] + a[15] * b[15];
	}
	inline void MultMatrix(float* a, float* b) {
		float sum[16]; MultMatrixTo(sum, a, b);
		memcpy(a, sum, sizeof(sum));
	}

	void SetPerspective(float FOV, float aspect, float Znear, float Zfar);
	void MultRotate(float angle, float x, float y, float z);
	
    inline void normalize(int side) {
		float magnitude = sqrtf(frus[side + 0] * frus[side + 0] + frus[side + 1] * frus[side + 1] + frus[side + 2] * frus[side + 2]);
		frus[side + 0] /= magnitude; frus[side + 1] /= magnitude; frus[side + 2] /= magnitude; frus[side + 3] /= magnitude;
	}
	
    void update();
	bool FrustumTest(const ChunkBox& aabb);
}

#endif

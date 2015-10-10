#include "Frustum.h"

namespace Frustum {
	double frus[24];
	double proj[16], modl[16];
	double clip[16];

	void normalize(int side) {
		double magnitude = sqrt(frus[side + 0] * frus[side + 0] + frus[side + 1] * frus[side + 1] + frus[side + 2] * frus[side + 2]);

		frus[side + 0] /= magnitude;
		frus[side + 1] /= magnitude;
		frus[side + 2] /= magnitude;
		frus[side + 3] /= magnitude;
	}

	void calc() {

		glGetDoublev(GL_PROJECTION_MATRIX, proj);
		glGetDoublev(GL_MODELVIEW_MATRIX, modl);

		clip[0] = modl[0] * proj[0] + modl[1] * proj[4] + modl[2] * proj[8] + modl[3] * proj[12];
		clip[1] = modl[0] * proj[1] + modl[1] * proj[5] + modl[2] * proj[9] + modl[3] * proj[13];
		clip[2] = modl[0] * proj[2] + modl[1] * proj[6] + modl[2] * proj[10] + modl[3] * proj[14];
		clip[3] = modl[0] * proj[3] + modl[1] * proj[7] + modl[2] * proj[11] + modl[3] * proj[15];

		clip[4] = modl[4] * proj[0] + modl[5] * proj[4] + modl[6] * proj[8] + modl[7] * proj[12];
		clip[5] = modl[4] * proj[1] + modl[5] * proj[5] + modl[6] * proj[9] + modl[7] * proj[13];
		clip[6] = modl[4] * proj[2] + modl[5] * proj[6] + modl[6] * proj[10] + modl[7] * proj[14];
		clip[7] = modl[4] * proj[3] + modl[5] * proj[7] + modl[6] * proj[11] + modl[7] * proj[15];

		clip[8] = modl[8] * proj[0] + modl[9] * proj[4] + modl[10] * proj[8] + modl[11] * proj[12];
		clip[9] = modl[8] * proj[1] + modl[9] * proj[5] + modl[10] * proj[9] + modl[11] * proj[13];
		clip[10] = modl[8] * proj[2] + modl[9] * proj[6] + modl[10] * proj[10] + modl[11] * proj[14];
		clip[11] = modl[8] * proj[3] + modl[9] * proj[7] + modl[10] * proj[11] + modl[11] * proj[15];

		clip[12] = modl[12] * proj[0] + modl[13] * proj[4] + modl[14] * proj[8] + modl[15] * proj[12];
		clip[13] = modl[12] * proj[1] + modl[13] * proj[5] + modl[14] * proj[9] + modl[15] * proj[13];
		clip[14] = modl[12] * proj[2] + modl[13] * proj[6] + modl[14] * proj[10] + modl[15] * proj[14];
		clip[15] = modl[12] * proj[3] + modl[13] * proj[7] + modl[14] * proj[11] + modl[15] * proj[15];

		frus[0] = clip[3] - clip[0];
		frus[1] = clip[7] - clip[4];
		frus[2] = clip[11] - clip[8];
		frus[3] = clip[15] - clip[12];

		normalize(0);

		frus[4] = clip[3] + clip[0];
		frus[5] = clip[7] + clip[4];
		frus[6] = clip[11] + clip[8];
		frus[7] = clip[15] + clip[12];

		normalize(4);

		frus[8] = clip[3] + clip[1];
		frus[9] = clip[7] + clip[5];
		frus[10] = clip[11] + clip[9];
		frus[11] = clip[15] + clip[13];

		normalize(8);

		frus[12] = clip[3] - clip[1];
		frus[13] = clip[7] - clip[5];
		frus[14] = clip[11] - clip[9];
		frus[15] = clip[15] - clip[13];

		normalize(12);

		frus[16] = clip[3] - clip[2];
		frus[17] = clip[7] - clip[6];
		frus[18] = clip[11] - clip[10];
		frus[19] = clip[15] - clip[14];

		normalize(16);

		frus[20] = clip[3] + clip[2];
		frus[21] = clip[7] + clip[6];
		frus[22] = clip[11] + clip[10];
		frus[23] = clip[15] + clip[14];

		normalize(20);

	}

	bool AABBInFrustum(const Hitbox::AABB& aabb) {
		for (int i = 0; i < 24; i += 4){
			if (frus[i + 0] * aabb.xmin + frus[i + 1] * aabb.ymin + frus[i + 2] * aabb.zmin + frus[i + 3] <= 0.0 &&
				frus[i + 0] * aabb.xmax + frus[i + 1] * aabb.ymin + frus[i + 2] * aabb.zmin + frus[i + 3] <= 0.0 &&
				frus[i + 0] * aabb.xmin + frus[i + 1] * aabb.ymax + frus[i + 2] * aabb.zmin + frus[i + 3] <= 0.0 &&
				frus[i + 0] * aabb.xmax + frus[i + 1] * aabb.ymax + frus[i + 2] * aabb.zmin + frus[i + 3] <= 0.0 &&
				frus[i + 0] * aabb.xmin + frus[i + 1] * aabb.ymin + frus[i + 2] * aabb.zmax + frus[i + 3] <= 0.0 &&
				frus[i + 0] * aabb.xmax + frus[i + 1] * aabb.ymin + frus[i + 2] * aabb.zmax + frus[i + 3] <= 0.0 &&
				frus[i + 0] * aabb.xmin + frus[i + 1] * aabb.ymax + frus[i + 2] * aabb.zmax + frus[i + 3] <= 0.0 &&
				frus[i + 0] * aabb.xmax + frus[i + 1] * aabb.ymax + frus[i + 2] * aabb.zmax + frus[i + 3] <= 0.0){
				return false;
			}
		}
		return true;
	}
}

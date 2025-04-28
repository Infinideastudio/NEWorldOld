#include "FrustumTest.h"
#include <memory>

void FrustumTest::LoadIdentity() {
	proj = Mat4f(1.0f);
	modl = Mat4f(1.0f);
}

void FrustumTest::MultPerspective(float fov, float aspect, float near, float far) {
	proj = Mat4f::perspective(fov, aspect, near, far) * proj;
	mFov = fov;
	mAspect = aspect;
	mNear = near;
	mFar = far;
}

void FrustumTest::MultOrtho(float left, float right, float bottom, float top, float near, float far) {
	modl = Mat4f::ortho(left, right, bottom, top, near, far) * modl;
}

void FrustumTest::MultRotate(float angle, float x, float y, float z) {
	modl = Mat4f::rotation(angle, Vec3f(x, y, z)) * modl;
}

inline void FrustumTest::normalize(int side) {
	float magnitude = std::sqrt(frus[side + 0] * frus[side + 0] + frus[side + 1] * frus[side + 1] + frus[side + 2] * frus[side + 2]);
	frus[side + 0] /= magnitude;
	frus[side + 1] /= magnitude;
	frus[side + 2] /= magnitude;
	frus[side + 3] /= magnitude;
}

void FrustumTest::update() {
	Mat4f mvp = proj * modl;
	mvp.transpose();

	frus[0] = mvp.data[3] - mvp.data[0];
	frus[1] = mvp.data[7] - mvp.data[4];
	frus[2] = mvp.data[11] - mvp.data[8];
	frus[3] = mvp.data[15] - mvp.data[12];
	normalize(0);

	frus[4] = mvp.data[3] + mvp.data[0];
	frus[5] = mvp.data[7] + mvp.data[4];
	frus[6] = mvp.data[11] + mvp.data[8];
	frus[7] = mvp.data[15] + mvp.data[12];
	normalize(4);

	frus[8] = mvp.data[3] + mvp.data[1];
	frus[9] = mvp.data[7] + mvp.data[5];
	frus[10] = mvp.data[11] + mvp.data[9];
	frus[11] = mvp.data[15] + mvp.data[13];
	normalize(8);

	frus[12] = mvp.data[3] - mvp.data[1];
	frus[13] = mvp.data[7] - mvp.data[5];
	frus[14] = mvp.data[11] - mvp.data[9];
	frus[15] = mvp.data[15] - mvp.data[13];
	normalize(12);

	frus[16] = mvp.data[3] - mvp.data[2];
	frus[17] = mvp.data[7] - mvp.data[6];
	frus[18] = mvp.data[11] - mvp.data[10];
	frus[19] = mvp.data[15] - mvp.data[14];
	normalize(16);

	frus[20] = mvp.data[3] + mvp.data[2];
	frus[21] = mvp.data[7] + mvp.data[6];
	frus[22] = mvp.data[11] + mvp.data[10];
	frus[23] = mvp.data[15] + mvp.data[14];
	normalize(20);
}

bool FrustumTest::test(const ChunkBox& aabb) {
	for (int i = 0; i < 24; i += 4) {
		if (frus[i] * aabb.xmin + frus[i + 1] * aabb.ymin + frus[i + 2] * aabb.zmin + frus[i + 3] <= 0.0f &&
			frus[i] * aabb.xmax + frus[i + 1] * aabb.ymin + frus[i + 2] * aabb.zmin + frus[i + 3] <= 0.0f &&
			frus[i] * aabb.xmin + frus[i + 1] * aabb.ymax + frus[i + 2] * aabb.zmin + frus[i + 3] <= 0.0f &&
			frus[i] * aabb.xmax + frus[i + 1] * aabb.ymax + frus[i + 2] * aabb.zmin + frus[i + 3] <= 0.0f &&
			frus[i] * aabb.xmin + frus[i + 1] * aabb.ymin + frus[i + 2] * aabb.zmax + frus[i + 3] <= 0.0f &&
			frus[i] * aabb.xmax + frus[i + 1] * aabb.ymin + frus[i + 2] * aabb.zmax + frus[i + 3] <= 0.0f &&
			frus[i] * aabb.xmin + frus[i + 1] * aabb.ymax + frus[i + 2] * aabb.zmax + frus[i + 3] <= 0.0f &&
			frus[i] * aabb.xmax + frus[i + 1] * aabb.ymax + frus[i + 2] * aabb.zmax + frus[i + 3] <= 0.0f) return false;
	}
	return true;
}

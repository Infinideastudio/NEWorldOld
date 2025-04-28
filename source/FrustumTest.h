#pragma once
#include <vector>
#include "Hitbox.h"
#include "Mat4.h"

class FrustumTest {
private:
	Mat4f proj = Mat4f(1.0f);
	Mat4f modl = Mat4f(1.0f);
	float frus[24];
	float mFov = 0, mAspect = 0, mNear = 0, mFar = 0;

public:
	// AABB with Float32 coords
	struct ChunkBox {
		float xmin, ymin, zmin;
		float xmax, ymax, zmax;
	};

	inline const float* getProjMatrix() const { return proj.data; }
	inline const float* getModlMatrix() const { return modl.data; }

	void LoadIdentity();
	void MultPerspective(float fov, float aspect, float near, float far);
	void MultOrtho(float left, float right, float bottom, float top, float near, float far);
	void MultRotate(float angle, float x, float y, float z);

	void normalize(int side);
	void update();
	bool test(const ChunkBox& aabb);

	float getFov() const { return mFov; }
	float getAspect() const { return mAspect; }
	float getNear() const { return mNear; }
	float getFar() const { return mFar; }
};

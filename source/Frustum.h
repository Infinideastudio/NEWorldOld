#pragma once

#include "Vec3.h"
#include "Mat4.h"

class Frustum {
public:
	Vec3f getPosition() const { return mPosition; }
	Vec3f getRotation() const { return mRotation; }
	void setPosition(const Vec3f& pos) { mPosition = pos; }
	void setRotation(const Vec3f& rot) { mRotation = rot; }
	void setPerspective(float fov, float aspect, float near_, float far_) {
		mFOV = fov, mAspect = aspect, mNear = near_, mFar = far_;
	}

	Mat4f getProjectionMatrix() const {
		return Mat4f::perspective(mFOV, mAspect, mNear, mFar);
	}
	Mat4f getModelViewMatrix() const {
		Mat4f res(1.0f);
		res *= Mat4f::rotation(-mRotation.x, Vec3f(1.0f, 0.0f, 0.0f));
		res *= Mat4f::rotation(-mRotation.y, Vec3f(0.0f, 1.0f, 0.0f));
		res *= Mat4f::rotation(-mRotation.z, Vec3f(0.0f, 0.0f, 1.0f));
		res *= Mat4f::translation(-mPosition);
		return res;
	}

private:
	Vec3f mPosition = { 0, 0, 0 }, mRotation = { 0, 0, 0 };
	float mFOV = 0, mAspect = 0, mNear = 0, mFar = 0;
};

#pragma once
#include "Mat4.h"

class FrustumTest {
private:
	float frus[24];

public:
	// AABB with 32-bit float coords
	struct AABBf {
		float xmin, ymin, zmin;
		float xmax, ymax, zmax;
	};

	explicit FrustumTest(Mat4f const& mvp);
	bool test(const AABBf& aabb) const;
};

#ifndef FRUSTUM_H
#define FRUSTUM_H
#include "Definitions.h"
#include "Hitbox.h"

namespace Frustum{
	void normalize(int side);
	void calc();
	bool AABBInFrustum(const Hitbox::AABB& aabb);
}

#endif
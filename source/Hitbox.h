#pragma once
#include "Definitions.h"

namespace Hitbox {

	struct AABB{
		//Axis Aligned Bounding Box
		double xmin;
		double ymin;
		double zmin;
		double xmax;
		double ymax;
		double zmax;
	};

	extern AABB Emptybox;
	extern bool stuck;

	bool inXclip(const AABB& boxA, const AABB& boxB);
	bool inYclip(const AABB& boxA, const AABB& boxB);
	bool inZclip(const AABB& boxA, const AABB& boxB);

	bool Hit(const AABB& boxA, const AABB& boxB);

	double MaxMoveOnXclip(const AABB& boxA, const AABB& boxB, double movedist);
	double MaxMoveOnYclip(const AABB& boxA, const AABB& boxB, double movedist);
	double MaxMoveOnZclip(const AABB& boxA, const AABB& boxB, double movedist);

	AABB Expand(const AABB& box, double xe, double ye, double ze);
	void Move(AABB &box, double xa, double ya, double za);
	void MoveTo(AABB &box, double x, double y, double z);
	void renderAABB(const AABB& box, float colR, float colG, float colB, int mode);
}
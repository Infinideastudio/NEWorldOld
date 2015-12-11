#pragma once
#include "Definitions.h"

namespace Hitbox
{
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

	bool inXclip(AABB& boxA, AABB& boxB);
	bool inYclip(AABB& boxA, AABB& boxB);
	bool inZclip(AABB& boxA, AABB& boxB);

	bool Hit(AABB& boxA, AABB& boxB);

	double MaxMoveOnXclip(AABB& boxA, AABB& boxB, double movedist);
	double MaxMoveOnYclip(AABB& boxA, AABB& boxB, double movedist);
	double MaxMoveOnZclip(AABB& boxA, AABB& boxB, double movedist);

	AABB Expand(AABB& box, double xe, double ye, double ze);
	void Move(AABB &box, double xa, double ya, double za);
	void MoveTo(AABB &box, double x, double y, double z);
	void renderAABB(AABB& box, float colR, float colG, float colB, int mode);
}
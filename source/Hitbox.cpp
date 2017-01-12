#include "Hitbox.h"

namespace Hitbox{

	bool stuck = false;
	AABB Emptybox;

	bool inXclip(const AABB& boxA, const AABB& boxB)
    {
        return
            (boxA.min.x > boxB.min.x && boxA.min.x < boxB.max.x) || (boxA.max.x > boxB.min.x && boxA.max.x < boxB.max.x) ||
            (boxB.min.x > boxA.min.x && boxB.min.x < boxA.max.x) || (boxB.max.x > boxA.min.x && boxB.max.x < boxA.max.x);
	}

	bool inYclip(const AABB& boxA, const AABB& boxB)
    {
        return
            (boxA.min.y > boxB.min.y && boxA.min.y < boxB.max.y) || (boxA.max.y > boxB.min.y && boxA.max.y < boxB.max.y) ||
            (boxB.min.y > boxA.min.y && boxB.min.y < boxA.max.y) || (boxB.max.y > boxA.min.y && boxB.max.y < boxA.max.y);
	}

	bool inZclip(const AABB& boxA, const AABB& boxB)
    {
        return
            (boxA.min.z > boxB.min.z && boxA.min.z < boxB.max.z) || (boxA.max.z > boxB.min.z && boxA.max.z < boxB.max.z) ||
            (boxB.min.z > boxA.min.z && boxB.min.z < boxA.max.z) || (boxB.max.z > boxA.min.z && boxB.max.z < boxA.max.z);
	}

	bool Hit(const AABB& boxA, const AABB& boxB)
    {
		return inXclip(boxA, boxB) && inYclip(boxA, boxB) && inZclip(boxA, boxB);
	}

	double MaxMoveOnXclip(const AABB& boxA, const AABB& boxB, double movedist){
		//用boxA去撞boxB，别搞反了
		double ret = 0.0;
		if (!(inYclip(boxA, boxB) && inZclip(boxA, boxB))){
			ret = movedist;
		}
		else if (boxA.min.x >= boxB.max.x && movedist < 0.00) {
			ret = boxB.max.x - boxA.min.x;
			if (ret<movedist) ret = movedist;
		}
		else if (boxA.max.x <= boxB.min.x && movedist > 0.0) {
			ret = boxB.min.x - boxA.max.x;
			if (ret > movedist) ret = movedist;
		}
		else{
			if (!stuck) ret = movedist;
			else ret = 0.0;
		}
		return ret;
	}

	double MaxMoveOnYclip(const AABB& boxA, const AABB& boxB, double movedist){
		//用boxA去撞boxB，别搞反了 （这好像是句废话）
		double ret = 0.0;
		if (!(inXclip(boxA, boxB) && inZclip(boxA, boxB))){
			ret = movedist;
		}
		else if (boxA.min.y >= boxB.max.y && movedist < 0.00) {
			ret = boxB.max.y - boxA.min.y;
			if (ret<movedist) ret = movedist;
		}
		else if (boxA.max.y <= boxB.min.y && movedist > 0.0) {
			ret = boxB.min.y - boxA.max.y;
			if (ret > movedist) ret = movedist;
		}
		else{
			if (!stuck) ret = movedist;
			else ret = 0.0;
		}
		return ret;
	}

	double MaxMoveOnZclip(const AABB& boxA, const AABB& boxB, double movedist){
		//用boxA去撞boxB，别搞反了 （这好像还是句废话）
		double ret = 0.0;
		if (!(inXclip(boxA, boxB) && inYclip(boxA, boxB))){
			ret = movedist;
		}
		else if (boxA.min.z >= boxB.max.z && movedist < 0.00) {
			ret = boxB.max.z - boxA.min.z;
			if (ret<movedist) ret = movedist;
		}
		else if (boxA.max.z <= boxB.min.z && movedist > 0.0) {
			ret = boxB.min.z - boxA.max.z;
			if (ret > movedist) ret = movedist;
		}
		else{
			if (!stuck) ret = movedist;
			else ret = 0.0;
		}
		return ret;
	}

	AABB Expand(const AABB& box, double xe, double ye, double ze){
		AABB ret = box;
		if (xe > 0.0)
			ret.max.x += xe;
		else
			ret.min.x += xe;

		if (ye > 0.0)
			ret.max.y += ye;
		else
			ret.min.y += ye;

		if (ze > 0.0)
			ret.max.z += ze;
		else
			ret.min.z += ze;

		return ret;
	}

	void Move(AABB &box, double xa, double ya, double za)
    {
        box.min += {xa, ya, za};
        box.max += {xa, ya, za};
	}

	void MoveTo(AABB &box, double x, double y, double z)
    {
		Vec3d half = (box.max - box.min) / 2;
        box.min = Vec3d{ x, y, z } - half;
        box.max = Vec3d{ x, y, z } + half;
	}
}

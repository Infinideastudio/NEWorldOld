#pragma once
#include "Definitions.h"
#include "math/vector.h"

namespace Hitbox
{
    //Axis Aligned Bounding Box
    struct AABB
    {
        Vec3d min, max;
        AABB() = default;
        AABB(const Vec3d& _min, const Vec3d& _max) :min(_min), max(_max) {}
        AABB Expand(double xe, double ye, double ze);
        void Move(double xa, double ya, double za);
        void MoveTo(double x, double y, double z);
    };

    bool hit(const AABB &boxA, const AABB &boxB);
    double maxMoveOnXclip(const AABB &boxA, const AABB &boxB, double movedist);
    double maxMoveOnYclip(const AABB &boxA, const AABB &boxB, double movedist);
    double maxMoveOnZclip(const AABB &boxA, const AABB &boxB, double movedist);
}

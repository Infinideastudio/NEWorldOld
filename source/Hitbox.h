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
        AABB Expand(double xe, double ye, double ze)
        {
            AABB ret(*this);

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
        void Move(double xa, double ya, double za)
        {
            min += {xa, ya, za};
            max += {xa, ya, za};
        }
        void MoveTo(double x, double y, double z)
        {
            Vec3d half = (max - min) / 2;
            min = Vec3d{ x, y, z } -half;
            max = Vec3d{ x, y, z } +half;
        }
    };

    template <size_t w>
    constexpr bool inClip(const AABB &boxA, const AABB &boxB)
    {
        return
            (boxA.min.data[w] > boxB.min.data[w] && boxA.min.data[w] < boxB.max.data[w]) || (boxA.max.data[w] > boxB.min.data[w] && boxA.max.data[w] < boxB.max.data[w]) ||
            (boxB.min.data[w] > boxA.min.data[w] && boxB.min.data[w] < boxA.max.data[w]) || (boxB.max.data[w] > boxA.min.data[w] && boxB.max.data[w] < boxA.max.data[w]);
    }

    template<size_t w, size_t A, size_t B>
    double maxMove(const AABB &boxA, const AABB &boxB, double movedist)
    {
        // boxAײboxB
        double ret = 0.0;
        if (!(inClip<A>(boxA, boxB) && inClip<B>(boxA, boxB)))
        {
            ret = movedist;
        }
        else if (boxA.min.data[w] >= boxB.max.data[w] && movedist < 0.00)
        {
            ret = std::max(boxB.max.data[w] - boxA.min.data[w], movedist);
        }
        else if (boxA.max.data[w] <= boxB.min.data[w] && movedist > 0.0)
        {
            ret = std::min(boxB.min.data[w] - boxA.max.data[w], movedist);
        }
        return ret;
    }

    constexpr bool hit(const AABB &boxA, const AABB &boxB)
    {
        return inClip<0>(boxA, boxB) && inClip<1>(boxA, boxB) && inClip<2>(boxA, boxB);
    }
    inline double maxMoveOnXclip(const AABB &boxA, const AABB &boxB, double movedist)
    {
        return maxMove<0, 1, 2>(boxA, boxB, movedist);
    }
    inline double maxMoveOnYclip(const AABB &boxA, const AABB &boxB, double movedist)
    {
        return maxMove<1, 0, 2>(boxA, boxB, movedist);
    }
    inline double maxMoveOnZclip(const AABB &boxA, const AABB &boxB, double movedist)
    {
        return maxMove<2, 0, 1>(boxA, boxB, movedist);
    }
}

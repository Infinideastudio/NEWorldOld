#include "Definitions.h"
#include "Hitbox.h"

namespace Frustum
{
    void LoadIdentity();
    void MultMatrix(float *m);
    void setPerspective(float FOV, float aspect, float Znear, float Zfar);
    void multRotate(float angle, float x, float y, float z);
    void normalize(int side);
    void calc();
    bool AABBInFrustum(const Hitbox::AABB &aabb);
}


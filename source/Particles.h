#pragma once
#include "Definitions.h"
#include "Hitbox.h"

namespace particles
{
    class Particle
    {
    public:
        bool exist = false;
        double xpos, ypos, zpos;
        float xsp, ysp, zsp, psize, tcX, tcY;
        int lasts;
        Hitbox::AABB hb;
    };
    void update(Particle &ptc);
    void updateall();
    void render(Particle &ptc);
    void renderall(double xpos, double ypos, double zpos);
    void throwParticle(block pt, float x, float y, float z, float xs, float ys, float zs, float psz, int last);
}

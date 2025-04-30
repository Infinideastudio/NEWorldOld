#pragma once
#include "Definitions.h"
#include "Hitbox.h"

namespace Particles {
    const int PARTICALE_MAX = 4096;
    struct Particle{
        double xpos, ypos, zpos;
        float xsp, ysp, zsp, psize;
        int lasts;
        BlockID bl;
        TextureIndex tex;
        float tcx, tcy;
        Hitbox::AABB hb;
    };
    extern std::vector<Particle> ptcs;
    extern int ptcsrendered;
    void update(Particle& ptc);
    void updateall();
    void renderall(double xpos, double ypos, double zpos, double interp);
    void throwParticle(BlockID pt, float x, float y, float z, float xs, float ys, float zs, float psz, int last);
}

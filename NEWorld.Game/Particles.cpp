#include "Particles.h"
#include "Universe/World/World.h"
#include "Textures.h"

namespace Particles {
    std::vector<Particle> ptcs;
    int ptcsrendered;
    double pxpos, pypos, pzpos;

    void update(Particle &ptc) {

        const auto psz = ptc.psize;

        ptc.hb.xmin = ptc.xpos - psz;
        ptc.hb.xmax = ptc.xpos + psz;
        ptc.hb.ymin = ptc.ypos - psz;
        ptc.hb.ymax = ptc.ypos + psz;
        ptc.hb.zmin = ptc.zpos - psz;
        ptc.hb.zmax = ptc.zpos + psz;

        double dx = ptc.xsp;
        double dy = ptc.ysp;
        double dz = ptc.zsp;

        auto Hitboxes = World::getHitboxes(Hitbox::Expand(ptc.hb, dx, dy, dz));
        const int hitnum = Hitboxes.size();
        for (auto i = 0; i < hitnum; i++) {
            dy = Hitbox::MaxMoveOnYclip(ptc.hb, Hitboxes[i], dy);
        }
        Hitbox::Move(ptc.hb, 0.0, dy, 0.0);
        for (auto i = 0; i < hitnum; i++) {
            dx = Hitbox::MaxMoveOnXclip(ptc.hb, Hitboxes[i], dx);
        }
        Hitbox::Move(ptc.hb, dx, 0.0, 0.0);
        for (auto i = 0; i < hitnum; i++) {
            dz = Hitbox::MaxMoveOnZclip(ptc.hb, Hitboxes[i], dz);
        }
        Hitbox::Move(ptc.hb, 0.0, 0.0, dz);

        ptc.xpos += dx;
        ptc.ypos += dy;
        ptc.zpos += dz;
        if (dy != ptc.ysp) ptc.ysp = 0.0;
        ptc.xsp *= 0.6f;
        ptc.zsp *= 0.6f;
        ptc.ysp -= 0.01f;
        ptc.lasts -= 1;

    }

    void updateall() {
        for (auto iter = ptcs.begin(); iter < ptcs.end();) {
            if (!iter->exist) continue;
            update(*iter);
            if (iter->lasts <= 0) {
                iter->exist = false;
                iter = ptcs.erase(iter);
            } else {
                iter++;
            }
        }
    }

    void render(Particle &ptc) {
        //if (!Frustum::aabbInFrustum(ptc.hb)) return;
        ptcsrendered++;
        const auto size = static_cast<float>(BLOCKTEXTURE_UNITSIZE) / BLOCKTEXTURE_SIZE * (ptc.psize <= 1.0f ? ptc.psize : 1.0f);
        const auto col = World::getbrightness(RoundInt(ptc.xpos), RoundInt(ptc.ypos), RoundInt(ptc.zpos)) /
                    static_cast<float>(World::BRIGHTNESSMAX);
        const auto col1 = col * 0.5f;
        const auto col2 = col * 0.7f;
        const auto tcx = ptc.tcX;
        const auto tcy = ptc.tcY;
        const auto psize = ptc.psize;
        const auto palpha = (ptc.lasts < 30 ? ptc.lasts / 30.0 : 1.0);
        const auto xpos = ptc.xpos - pxpos;
        const auto ypos = ptc.ypos - pypos;
        const auto zpos = ptc.zpos - pzpos;

        glBegin(GL_QUADS);
        glColor4d(col1, col1, col1, palpha);
        glTexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
        glVertex3d(xpos - psize, ypos - psize, zpos + psize);
        glTexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
        glVertex3d(xpos + psize, ypos - psize, zpos + psize);
        glTexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
        glVertex3d(xpos + psize, ypos + psize, zpos + psize);
        glTexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
        glVertex3d(xpos - psize, ypos + psize, zpos + psize);

        glColor4d(col1, col1, col1, palpha);
        glTexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
        glVertex3d(xpos - psize, ypos + psize, zpos - psize);
        glTexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
        glVertex3d(xpos + psize, ypos + psize, zpos - psize);
        glTexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
        glVertex3d(xpos + psize, ypos - psize, zpos - psize);
        glTexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
        glVertex3d(xpos - psize, ypos - psize, zpos - psize);

        glColor4d(col, col, col, palpha);
        glTexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
        glVertex3d(xpos + psize, ypos + psize, zpos - psize);
        glTexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
        glVertex3d(xpos - psize, ypos + psize, zpos - psize);
        glTexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
        glVertex3d(xpos - psize, ypos + psize, zpos + psize);
        glTexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
        glVertex3d(xpos + psize, ypos + psize, zpos + psize);

        glColor4d(col, col, col, palpha);
        glTexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
        glVertex3d(xpos - psize, ypos - psize, zpos - psize);
        glTexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
        glVertex3d(xpos + psize, ypos - psize, zpos - psize);
        glTexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
        glVertex3d(xpos + psize, ypos - psize, zpos + psize);
        glTexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
        glVertex3d(xpos - psize, ypos - psize, zpos + psize);

        glColor4d(col2, col2, col2, palpha);
        glTexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
        glVertex3d(xpos + psize, ypos + psize, zpos - psize);
        glTexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
        glVertex3d(xpos + psize, ypos + psize, zpos + psize);
        glTexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
        glVertex3d(xpos + psize, ypos - psize, zpos + psize);
        glTexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
        glVertex3d(xpos + psize, ypos - psize, zpos - psize);

        glColor4d(col2, col2, col2, palpha);
        glTexCoord2d(tcx + size * 0.0, tcy + size * 0.0);
        glVertex3d(xpos - psize, ypos - psize, zpos - psize);
        glTexCoord2d(tcx + size * 1.0, tcy + size * 0.0);
        glVertex3d(xpos - psize, ypos - psize, zpos + psize);
        glTexCoord2d(tcx + size * 1.0, tcy + size * 1.0);
        glVertex3d(xpos - psize, ypos + psize, zpos + psize);
        glTexCoord2d(tcx + size * 0.0, tcy + size * 1.0);
        glVertex3d(xpos - psize, ypos + psize, zpos - psize);
        glEnd();
    }

    void renderall(double xpos, double ypos, double zpos) {
        pxpos = xpos;
        pypos = ypos;
        pzpos = zpos;
        ptcsrendered = 0;
        for (unsigned int i = 0; i != ptcs.size(); i++) {
            if (!ptcs[i].exist) continue;
            render(ptcs[i]);
        }
    }

    void throwParticle(Block pt, float x, float y, float z, float xs, float ys, float zs, float psz, int last) {
        const auto tcX1 = static_cast<float>(Textures::getTexcoordX(pt, 2));
        const auto tcY1 = static_cast<float>(Textures::getTexcoordY(pt, 2));
        Particle ptc;
        ptc.exist = true;
        ptc.xpos = x;
        ptc.ypos = y;
        ptc.zpos = z;
        ptc.xsp = xs;
        ptc.ysp = ys;
        ptc.zsp = zs;
        ptc.psize = psz;
        ptc.hb.xmin = x - psz;
        ptc.hb.xmax = x + psz;
        ptc.hb.ymin = y - psz;
        ptc.hb.ymax = y + psz;
        ptc.hb.zmin = z - psz;
        ptc.hb.zmax = z + psz;
        ptc.lasts = last;
        ptc.tcX = tcX1 + static_cast<float>(rnd()) * (static_cast<float>(BLOCKTEXTURE_UNITSIZE) / BLOCKTEXTURE_SIZE) * (1.0f - psz);
        ptc.tcY = tcY1 + static_cast<float>(rnd()) * (static_cast<float>(BLOCKTEXTURE_UNITSIZE) / BLOCKTEXTURE_SIZE) * (1.0f - psz);
        ptcs.push_back(ptc);
    }
}

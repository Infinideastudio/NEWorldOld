#include "Particles.h"
#include "World.h"
#include "Textures.h"
#include "Renderer.h"
#include "Player.h"

namespace particles
{
    vector<Particle> ptcs;
    double pxpos, pypos, pzpos;

    void update(Particle &ptc)
    {
        double dx, dy, dz;
        float psz = ptc.psize;

        ptc.hb.min.x = ptc.xpos - psz;
        ptc.hb.max.x = ptc.xpos + psz;
        ptc.hb.min.y = ptc.ypos - psz;
        ptc.hb.max.y = ptc.ypos + psz;
        ptc.hb.min.z = ptc.zpos - psz;
        ptc.hb.max.z = ptc.zpos + psz;

        dx = ptc.xsp;
        dy = ptc.ysp;
        dz = ptc.zsp;

        vector<Hitbox::AABB> Hitboxes = world::getHitboxes(ptc.hb.Expand(dx, dy, dz));
       
        for (const auto& box : Hitboxes)
            dy = Hitbox::maxMoveOnYclip(ptc.hb, box, dy);

        ptc.hb.Move(0.0, dy, 0.0);

        for (const auto& box : Hitboxes)
            dx = Hitbox::maxMoveOnXclip(ptc.hb, box, dx);

        ptc.hb.Move(dx, 0.0, 0.0);

        for (const auto& box : Hitboxes)
            dz = Hitbox::maxMoveOnZclip(ptc.hb, box, dz);
        
        ptc.hb.Move(0.0, 0.0, dz);

        ptc.xpos += dx;
        ptc.ypos += dy;
        ptc.zpos += dz;

        if (dy != ptc.ysp)
        {
            ptc.ysp = 0.0;
        }

        ptc.xsp *= 0.6f;
        ptc.zsp *= 0.6f;
        ptc.ysp -= 0.01f;
        ptc.lasts -= 1;
    }

    void updateall()
    {
        for (auto iter = ptcs.begin(); iter < ptcs.end();)
        {
            update(*iter);
            if (iter->lasts <= 0)
                iter = ptcs.erase(iter);
            else
                ++iter;
        }
    }

    void render(Particle &ptc)
    {
        float size = (float)BLOCKTEXTURE_UNITSIZE / BLOCKTEXTURE_SIZE * ptc.psize;
        float col = world::getbrightness(RoundInt(ptc.xpos), RoundInt(ptc.ypos), RoundInt(ptc.zpos)) / (float)world::BRIGHTNESSMAX;
        float col1 = col * 0.5f;
        float col2 = col * 0.7f;
        float tcx = ptc.tcX;
        float tcy = ptc.tcY;
        float psize = ptc.psize;
        double palpha = (ptc.lasts < 30 ? ptc.lasts / 30.0 : 1.0);
        double xpos = ptc.xpos - pxpos;
        double ypos = ptc.ypos - pypos;
        double zpos = ptc.zpos - pzpos;

        glBegin(GL_QUADS);
        glColor4d(col1, col1, col1, palpha);
        glTexCoord2d(tcx, tcy);
        glVertex3d(xpos - psize, ypos - psize, zpos + psize);
        glTexCoord2d(tcx + size, tcy);
        glVertex3d(xpos + psize, ypos - psize, zpos + psize);
        glTexCoord2d(tcx + size, tcy + size);
        glVertex3d(xpos + psize, ypos + psize, zpos + psize);
        glTexCoord2d(tcx, tcy + size);
        glVertex3d(xpos - psize, ypos + psize, zpos + psize);

        glColor4d(col1, col1, col1, palpha);
        glTexCoord2d(tcx, tcy);
        glVertex3d(xpos - psize, ypos + psize, zpos - psize);
        glTexCoord2d(tcx + size, tcy);
        glVertex3d(xpos + psize, ypos + psize, zpos - psize);
        glTexCoord2d(tcx + size, tcy + size);
        glVertex3d(xpos + psize, ypos - psize, zpos - psize);
        glTexCoord2d(tcx, tcy + size);
        glVertex3d(xpos - psize, ypos - psize, zpos - psize);

        glColor4d(col, col, col, palpha);
        glTexCoord2d(tcx, tcy);
        glVertex3d(xpos + psize, ypos + psize, zpos - psize);
        glTexCoord2d(tcx + size, tcy);
        glVertex3d(xpos - psize, ypos + psize, zpos - psize);
        glTexCoord2d(tcx + size, tcy + size);
        glVertex3d(xpos - psize, ypos + psize, zpos + psize);
        glTexCoord2d(tcx, tcy + size);
        glVertex3d(xpos + psize, ypos + psize, zpos + psize);

        glColor4d(col, col, col, palpha);
        glTexCoord2d(tcx, tcy);
        glVertex3d(xpos - psize, ypos - psize, zpos - psize);
        glTexCoord2d(tcx + size, tcy);
        glVertex3d(xpos + psize, ypos - psize, zpos - psize);
        glTexCoord2d(tcx + size, tcy + size);
        glVertex3d(xpos + psize, ypos - psize, zpos + psize);
        glTexCoord2d(tcx, tcy + size);
        glVertex3d(xpos - psize, ypos - psize, zpos + psize);

        glColor4d(col2, col2, col2, palpha);
        glTexCoord2d(tcx, tcy);
        glVertex3d(xpos + psize, ypos + psize, zpos - psize);
        glTexCoord2d(tcx + size, tcy);
        glVertex3d(xpos + psize, ypos + psize, zpos + psize);
        glTexCoord2d(tcx + size, tcy + size);
        glVertex3d(xpos + psize, ypos - psize, zpos + psize);
        glTexCoord2d(tcx, tcy + size);
        glVertex3d(xpos + psize, ypos - psize, zpos - psize);

        glColor4d(col2, col2, col2, palpha);
        glTexCoord2d(tcx, tcy);
        glVertex3d(xpos - psize, ypos - psize, zpos - psize);
        glTexCoord2d(tcx + size, tcy);
        glVertex3d(xpos - psize, ypos - psize, zpos + psize);
        glTexCoord2d(tcx + size, tcy + size);
        glVertex3d(xpos - psize, ypos + psize, zpos + psize);
        glTexCoord2d(tcx, tcy + size);
        glVertex3d(xpos - psize, ypos + psize, zpos - psize);
        glEnd();
    }

    void renderall(double xpos, double ypos, double zpos)
    {
        pxpos = xpos;
        pypos = ypos;
        pzpos = zpos;
        for (unsigned int i = 0; i != ptcs.size(); i++)
        {
            if (!ptcs[i].exist)
            {
                continue;
            }

            render(ptcs[i]);
        }
    }

    void throwParticle(block pt, float x, float y, float z, float xs, float ys, float zs, float psz, int last)
    {
        float tcX1 = (float)Textures::getTexcoordX(pt, 2);
        float tcY1 = (float)Textures::getTexcoordY(pt, 2);
        Particle ptc;
        ptc.exist = true;
        ptc.xpos = x;
        ptc.ypos = y;
        ptc.zpos = z;
        ptc.xsp = xs;
        ptc.ysp = ys;
        ptc.zsp = zs;
        ptc.psize = psz;
        ptc.hb.min.x = x - psz;
        ptc.hb.max.x = x + psz;
        ptc.hb.min.y = y - psz;
        ptc.hb.max.y = y + psz;
        ptc.hb.min.z = z - psz;
        ptc.hb.max.z = z + psz;
        ptc.lasts = last;
        ptc.tcX = tcX1 + (float)rnd() * ((float)BLOCKTEXTURE_UNITSIZE / BLOCKTEXTURE_SIZE) * (1.0f - psz);
        ptc.tcY = tcY1 + (float)rnd() * ((float)BLOCKTEXTURE_UNITSIZE / BLOCKTEXTURE_SIZE) * (1.0f - psz);
        ptcs.push_back(ptc);
    }
}

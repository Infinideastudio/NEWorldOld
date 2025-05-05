module;

#include <glad/gl.h>
#undef assert

export module particles;
import std;
import types;
import math;
import blocks;
import rendering;
import textures;
import worlds;
import globals;

export namespace particles {

constexpr int PARTICALE_MAX = 4096;

struct Particle {
    double xpos = 0.0, ypos = 0.0, zpos = 0.0;
    double xsp = 0.0, ysp = 0.0, zsp = 0.0, psize = 0.0;
    int lasts = 0;
    blocks::Id bl = base_blocks().air;
    TextureIndex tex = TextureIndex::NULLBLOCK;
    float tcx = 0.0f, tcy = 0.0f;
};

std::vector<Particle> ptcs;
int ptcsrendered;
double pxpos, pypos, pzpos;

void update(worlds::World& world, Particle& ptc) {
    auto position = Vec3d(ptc.xpos, ptc.ypos, ptc.zpos);
    auto velocity = Vec3d(ptc.xsp, ptc.ysp, ptc.zsp);

    velocity.x() *= 0.6;
    velocity.z() *= 0.6;
    velocity.y() -= 0.03;
    auto velocity_original = velocity;

    auto particle_box = AABB3d(position - ptc.psize, position + ptc.psize);
    velocity = particle_box.clip_displacement(world.hitboxes(particle_box.extend(velocity)), velocity);
    position += velocity;

    ptc.xpos = position.x();
    ptc.ypos = position.y();
    ptc.zpos = position.z();
    ptc.xsp = velocity.x();
    ptc.ysp = velocity.y();
    ptc.zsp = velocity.z();
    ptc.lasts -= 1;
}

void update_all(worlds::World& world) {
    std::vector<Particle> next;
    for (auto& ptc: ptcs) {
        update(world, ptc);
        if (ptc.lasts > 0)
            next.emplace_back(std::move(ptc));
    }
    ptcs = std::move(next);
}

void mesh(worlds::World& world, Particle const& ptc, double interp) {
    auto psize = ptc.psize;
    auto bl = static_cast<float>(ptc.bl.get());
    auto tex = static_cast<float>(ptc.tex);
    auto tcx = ptc.tcx;
    auto tcy = ptc.tcy;
    // auto palpha = (ptc.lasts < 30 ? ptc.lasts / 30.0f : 1.0f);
    auto xpos = static_cast<float>(ptc.xpos - ptc.xsp + ptc.xsp * interp - pxpos);
    auto ypos = static_cast<float>(ptc.ypos - ptc.ysp + ptc.ysp * interp - pypos);
    auto zpos = static_cast<float>(ptc.zpos - ptc.zsp + ptc.zsp * interp - pzpos);

    auto col = world.block(Vec3i(std::lround(ptc.xpos), std::lround(ptc.ypos), std::lround(ptc.zpos)))->light.mixed();
    auto col1 = col, col2 = col;
    if (!AdvancedRender) {
        col1 *= 0.5f;
        col2 *= 0.7f;
    }
    Renderer::TexCoord3f(0.0f, 0.0f, tex);

    if (AdvancedRender) {
        Renderer::Normal3f(1.0f, 0.0f, 0.0f);
        Renderer::Attrib1f(bl);
    }
    Renderer::Color3f(col2, col2, col2);
    Renderer::TexCoord2f(tcx, tcy);
    Renderer::Vertex3f(xpos + psize, ypos + psize, zpos - psize);
    Renderer::TexCoord2f(tcx + psize, tcy);
    Renderer::Vertex3f(xpos + psize, ypos + psize, zpos + psize);
    Renderer::TexCoord2f(tcx + psize, tcy + psize);
    Renderer::Vertex3f(xpos + psize, ypos - psize, zpos + psize);
    Renderer::TexCoord2f(tcx, tcy + psize);
    Renderer::Vertex3f(xpos + psize, ypos - psize, zpos - psize);

    if (AdvancedRender) {
        Renderer::Normal3f(-1.0f, 0.0f, 0.0f);
        Renderer::Attrib1f(bl);
    }
    Renderer::Color3f(col2, col2, col2);
    Renderer::TexCoord2f(tcx, tcy);
    Renderer::Vertex3f(xpos - psize, ypos - psize, zpos - psize);
    Renderer::TexCoord2f(tcx + psize, tcy);
    Renderer::Vertex3f(xpos - psize, ypos - psize, zpos + psize);
    Renderer::TexCoord2f(tcx + psize, tcy + psize);
    Renderer::Vertex3f(xpos - psize, ypos + psize, zpos + psize);
    Renderer::TexCoord2f(tcx, tcy + psize);
    Renderer::Vertex3f(xpos - psize, ypos + psize, zpos - psize);

    if (AdvancedRender) {
        Renderer::Normal3f(0.0f, 1.0f, 0.0f);
        Renderer::Attrib1f(bl);
    }
    Renderer::Color3f(col, col, col);
    Renderer::TexCoord2f(tcx, tcy);
    Renderer::Vertex3f(xpos + psize, ypos + psize, zpos - psize);
    Renderer::TexCoord2f(tcx + psize, tcy);
    Renderer::Vertex3f(xpos - psize, ypos + psize, zpos - psize);
    Renderer::TexCoord2f(tcx + psize, tcy + psize);
    Renderer::Vertex3f(xpos - psize, ypos + psize, zpos + psize);
    Renderer::TexCoord2f(tcx, tcy + psize);
    Renderer::Vertex3f(xpos + psize, ypos + psize, zpos + psize);

    if (AdvancedRender) {
        Renderer::Normal3f(0.0f, -1.0f, 0.0f);
        Renderer::Attrib1f(bl);
    }
    Renderer::Color3f(col, col, col);
    Renderer::TexCoord2f(tcx, tcy);
    Renderer::Vertex3f(xpos - psize, ypos - psize, zpos - psize);
    Renderer::TexCoord2f(tcx + psize, tcy);
    Renderer::Vertex3f(xpos + psize, ypos - psize, zpos - psize);
    Renderer::TexCoord2f(tcx + psize, tcy + psize);
    Renderer::Vertex3f(xpos + psize, ypos - psize, zpos + psize);
    Renderer::TexCoord2f(tcx, tcy + psize);
    Renderer::Vertex3f(xpos - psize, ypos - psize, zpos + psize);

    if (AdvancedRender) {
        Renderer::Normal3f(0.0f, 0.0f, 1.0f);
        Renderer::Attrib1f(bl);
    }
    Renderer::Color3f(col1, col1, col1);
    Renderer::TexCoord2f(tcx, tcy);
    Renderer::Vertex3f(xpos - psize, ypos - psize, zpos + psize);
    Renderer::TexCoord2f(tcx + psize, tcy);
    Renderer::Vertex3f(xpos + psize, ypos - psize, zpos + psize);
    Renderer::TexCoord2f(tcx + psize, tcy + psize);
    Renderer::Vertex3f(xpos + psize, ypos + psize, zpos + psize);
    Renderer::TexCoord2f(tcx, tcy + psize);
    Renderer::Vertex3f(xpos - psize, ypos + psize, zpos + psize);

    if (AdvancedRender) {
        Renderer::Normal3f(0.0f, 0.0f, -1.0f);
        Renderer::Attrib1f(bl);
    }
    Renderer::Color3f(col1, col1, col1);
    Renderer::TexCoord2f(tcx, tcy);
    Renderer::Vertex3f(xpos - psize, ypos + psize, zpos - psize);
    Renderer::TexCoord2f(tcx + psize, tcy);
    Renderer::Vertex3f(xpos + psize, ypos + psize, zpos - psize);
    Renderer::TexCoord2f(tcx + psize, tcy + psize);
    Renderer::Vertex3f(xpos + psize, ypos - psize, zpos - psize);
    Renderer::TexCoord2f(tcx, tcy + psize);
    Renderer::Vertex3f(xpos - psize, ypos - psize, zpos - psize);
}

void render_all(worlds::World& world, Vec3d center, double interp) {
    pxpos = center.x();
    pypos = center.y();
    pzpos = center.z();
    ptcsrendered = 0;

    if (AdvancedRender)
        Renderer::Begin(GL_QUADS, 3, 3, 1, 3, 1);
    else
        Renderer::Begin(GL_QUADS, 3, 3, 1);
    for (auto const& ptc: ptcs) {
        mesh(world, ptc, interp);
        ptcsrendered++;
    }
    Renderer::End().render();
}

void throw_particle(blocks::Id pt, float x, float y, float z, float xs, float ys, float zs, float psz, int last) {
    Particle ptc;
    ptc.xpos = x;
    ptc.ypos = y;
    ptc.zpos = z;
    ptc.xsp = xs;
    ptc.ysp = ys;
    ptc.zsp = zs;
    ptc.psize = psz;
    ptc.lasts = last;
    ptc.bl = pt;
    ptc.tex = Textures::getTextureIndex(pt, 1);
    ptc.tcx = static_cast<float>(rnd()) * (1.0f - psz);
    ptc.tcy = static_cast<float>(rnd()) * (1.0f - psz);
    ptcs.emplace_back(std::move(ptc));
}
}

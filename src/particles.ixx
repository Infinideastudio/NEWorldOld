export module particles;
import std;
import types;
import math;
import blocks;
import render;
import rendering;
import textures;
import worlds;
import globals;

namespace spec = render::attrib_layout::spec;
using render::VertexArray;
using AttribIndexBuilder = decltype(Renderer::chunk_vertex_builder());

constexpr auto coords = std::array<std::array<Vec3f, 4>, 6>({
    {{{1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}}, // Right
    {{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}}}, // Left
    {{{0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Top
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}}, // Bottom
    {{{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}}}, // Front
    {{{1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}}}, // Back
});

constexpr auto tex_coords = std::array<std::array<Vec3f, 4>, 6>({
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Right
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Left
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Top
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Bottom
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Front
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Back
});

constexpr auto tangents = std::array<Vec3i8, 6>({
    { 0, 0, -1}, // Right
    { 0, 0, +1}, // Left
    {+1, 0,  0}, // Top
    {+1, 0,  0}, // Bottom
    {+1, 0,  0}, // Front
    {-1, 0,  0}, // Back
});

constexpr auto bitangents = std::array<Vec3i8, 6>({
    {0, +1,  0}, // Right
    {0, +1,  0}, // Left
    {0,  0, -1}, // Top
    {0,  0, +1}, // Bottom
    {0, +1,  0}, // Front
    {0, +1,  0}, // Back
});

export namespace particles {

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

void mesh(worlds::World& world, Particle const& ptc, double interp, AttribIndexBuilder& v) {
    auto psize = static_cast<float>(ptc.psize);
    auto bl = ptc.bl.get();
    auto tex = static_cast<float>(ptc.tex);
    auto tcx = ptc.tcx;
    auto tcy = ptc.tcy;
    // auto palpha = (ptc.lasts < 30 ? ptc.lasts / 30.0f : 1.0f);
    auto x = static_cast<float>(ptc.xpos - ptc.xsp + ptc.xsp * interp - pxpos);
    auto y = static_cast<float>(ptc.ypos - ptc.ysp + ptc.ysp * interp - pypos);
    auto z = static_cast<float>(ptc.zpos - ptc.zsp + ptc.zsp * interp - pzpos);

    auto light = world.block(Vec3f(ptc.xpos, ptc.ypos, ptc.zpos).floor<int32_t>())->light;
    auto br = std::max({light.sky(), light.block(), uint8_t{2}});

    for (auto direction = 0; direction < 6; direction++) {
        auto col = br * 255 / 15;
        switch (direction) {
            case 0:
                if (!AdvancedRender) {
                    col = col * 5 / 10;
                }
                break;
            case 1:
                if (!AdvancedRender) {
                    col = col * 5 / 10;
                }
                break;
            case 4:
                if (!AdvancedRender) {
                    col = col * 2 / 10;
                }
                break;
            case 5:
                if (!AdvancedRender) {
                    col = col * 2 / 10;
                }
                break;
        }
        v.material(bl);
        v.tangent(tangents[direction]);
        v.bitangent(bitangents[direction]);
        v.color(col);
        v.tex_coord(Vec3f(tcx, tcy, tex) + tex_coords[direction][0] * psize);
        v.coord(Vec3f(x, y, z) + coords[direction][0] * psize);
        v.color(col);
        v.tex_coord(Vec3f(tcx, tcy, tex) + tex_coords[direction][1] * psize);
        v.coord(Vec3f(x, y, z) + coords[direction][1] * psize);
        v.color(col);
        v.tex_coord(Vec3f(tcx, tcy, tex) + tex_coords[direction][2] * psize);
        v.coord(Vec3f(x, y, z) + coords[direction][2] * psize);
        v.color(col);
        v.tex_coord(Vec3f(tcx, tcy, tex) + tex_coords[direction][3] * psize);
        v.coord(Vec3f(x, y, z) + coords[direction][3] * psize);
        v.end_primitive();
    }
}

void render_all(worlds::World& world, Vec3d center, double interp) {
    pxpos = center.x();
    pypos = center.y();
    pzpos = center.z();
    ptcsrendered = 0;

    auto v = AttribIndexBuilder();
    for (auto const& ptc: ptcs) {
        mesh(world, ptc, interp, v);
        ptcsrendered++;
    }
    VertexArray::create(v, VertexArray::Primitive::TRIANGLE_FAN).first.render();
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

module worlds:worlds_rendering;
import :worlds;
import std;
import types;
import math;
import debug;
import chunks;
import rendering;
import render;

namespace worlds {

auto World::list_render_chunks(Vec3d center, int dist, double interp, std::optional<Frustumf> frustum)
    -> std::vector<RenderChunk> {
    auto ccenter = chunk_coord(Vec3i(center));
    auto res = std::vector<RenderChunk>{};
    for (auto const& [_, c]: _chunks) {
        if (!c->meshed())
            continue;
        if (!chunk_coord_out_of_range(c->coord(), ccenter, dist)) {
            if (!frustum || c->visible(center, *frustum)) {
                auto coord = Vec3d(c->coord() * chunks::Chunk::SIZE);
                coord -= Vec3d(0.0, c->load_anim() * std::pow(0.6, interp), 0.0);
                auto meshes = std::array{&c->mesh(0), &c->mesh(1)};
                res.emplace_back(coord, meshes);
            }
        }
    }
    return res;
}

void World::render_chunks(Vec3d center, std::vector<RenderChunk> const& crs, size_t index) {
    for (auto [coord, meshes]: crs) {
        coord -= center;
        auto const& [va, buffer] = *meshes[index];
        Renderer::shaders[Renderer::ActiveShader].setUniform("u_translation", Vec3f(coord));
        va.render();
    }
}

}

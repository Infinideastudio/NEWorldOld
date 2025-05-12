module worlds:worlds_rendering;
import :worlds;
import std;
import types;
import math;
import debug;
import chunks;
import render;
import rendering;

namespace worlds {

auto World::list_render_chunks(Vec3d center, int dist, double interp, std::optional<Frustumf> frustum)
    -> std::vector<RenderChunk> {
    auto ccenter = chunk_coord(Vec3i(center));
    auto res = std::vector<RenderChunk>{};
    for (auto const& [_, c]: _renders) {
        if (!c->meshed() || !c->mesh(0).first && !c->mesh(1).first
            || chunk_coord_out_of_range(c->coord(), ccenter, dist) || frustum && !c->visible(center, *frustum)) {
            continue;
        }
        auto coord = Vec3d(c->coord() * chunks::Chunk::SIZE);
        coord -= Vec3d(0.0, c->load_anim() * std::pow(0.6, interp), 0.0);
        auto meshes = std::array{&c->mesh(0), &c->mesh(1)};
        res.emplace_back(coord, meshes);
    }
    return res;
}

void World::render_chunks(Vec3d center, std::vector<RenderChunk> const& crs, size_t index) {
    auto usage = render::Buffer::Usage::WRITE;
    auto update = render::Buffer::Update::INFREQUENT;
    for (auto [coord, meshes]: crs) {
        coord -= center;
        auto const& [va, buffer] = *meshes[index];
        if (!va) {
            continue;
        }
        Renderer::model_uniforms.set<".u_translation">(coord);
        Renderer::model_uniform_buffer.write(Renderer::model_uniforms.bytes(), 0);
        va.render();
    }
    Renderer::model_uniforms.set<".u_translation">(0.0f, 0.0f, 0.0f);
    Renderer::model_uniform_buffer.write(Renderer::model_uniforms.bytes(), 0);
}

}

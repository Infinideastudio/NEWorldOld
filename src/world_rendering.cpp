module;

#include <array>
#include <cassert>
#include <cmath>
#include <optional>
#include <vector>

module worlds;
import frustum_tests;
import rendering;
import chunks;

auto World::ListRenderChunks(
    double x,
    double y,
    double z,
    int distance,
    double interp,
    std::optional<FrustumTest> frustum
) -> std::vector<RenderChunk> {
    auto ccenter = getChunkPos(Vec3i(x, y, z));
    auto res = std::vector<RenderChunk>{};
    for (auto const& [_, c]: chunks) {
        if (!c->meshed())
            continue;
        if (chunkInRange(c->coord(), ccenter, distance)) {
            if (!frustum || c->visible({x, y, z}, *frustum)) {
                res.emplace_back(*c.get(), static_cast<float>(interp));
            }
        }
    }
    return res;
}

void World::RenderChunks(double x, double y, double z, std::vector<RenderChunk> const& crs, size_t index) {
    assert(index < 2);
    for (auto const& cr: crs) {
        auto const& mesh = *cr.meshes[index];
        if (mesh.empty())
            continue;
        auto d = Vec3d(cr.ccoord) * 16.0 - Vec3d(x, y + cr.loadAnim, z);
        Renderer::shaders[Renderer::ActiveShader].setUniform("u_translation", Vec3f(d));
        mesh.render();
    }
}

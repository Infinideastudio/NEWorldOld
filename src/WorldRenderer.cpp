#include "WorldRenderer.h"

namespace WorldRenderer {
vector<RenderChunk> RenderChunkList;

void ListRenderChunks(double x, double y, double z, int distance, double interp, std::optional<FrustumTest> frustum) {
    int cx = getchunkpos((int) x);
    int cy = getchunkpos((int) y);
    int cz = getchunkpos((int) z);
    RenderChunkList.clear();
    for (auto const& [_, c]: World::chunks) {
        if (!c->meshed())
            continue;
        if (World::chunkInRange(c->x(), c->y(), c->z(), cx, cy, cz, distance)) {
            if (!frustum || c->visible({x, y, z}, *frustum)) {
                RenderChunkList.push_back(RenderChunk(c.get(), static_cast<float>(interp)));
            }
        }
    }
}

void RenderChunks(double x, double y, double z, int index) {
    assert(index >= 0 && index < 2);
    for (unsigned int i = 0; i < RenderChunkList.size(); i++) {
        RenderChunk const& cr = RenderChunkList[i];
        auto const& mesh = *cr.meshes[index];
        if (mesh.empty())
            continue;
        float xd = static_cast<float>(cr.cx * 16.0 - x);
        float yd = static_cast<float>(cr.cy * 16.0 - cr.loadAnim - y);
        float zd = static_cast<float>(cr.cz * 16.0 - z);
        Renderer::shaders[Renderer::ActiveShader].setUniform("u_translation", Vec3f(xd, yd, zd));
        mesh.render();
    }
}
}

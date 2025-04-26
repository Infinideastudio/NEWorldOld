#include "WorldRenderer.h"

namespace WorldRenderer {
    vector<RenderChunk> RenderChunkList;

    void ListRenderChunks(double x, double y, double z, int renderdistance, double interp, std::optional<FrustumTest> frustum) {
        int cx = getchunkpos((int)x);
        int cy = getchunkpos((int)y);
        int cz = getchunkpos((int)z);

        RenderChunkList.clear();
        if (frustum) World::Chunk::setVisibilityBase(x, y, z, *frustum);
        for (auto const& [_, c]: World::chunks) {
            if (!c->meshed()) continue;
            if (World::chunkInRange(c->x(), c->y(), c->z(), cx, cy, cz, renderdistance)) {
                if (!frustum || c->visible()) {
                    RenderChunkList.push_back(RenderChunk(c.get(), float(interp)));
                }
            }
        }
    }

    void RenderChunks(double x, double y, double z, int index) {
        assert(index >= 0 && index < 2);
        float m[16];
        memset(m, 0, sizeof(m));
        m[0] = m[5] = m[10] = m[15] = 1.0f;

        for (unsigned int i = 0; i < RenderChunkList.size(); i++) {
            RenderChunk const& cr = RenderChunkList[i];
            auto const& mesh = *cr.meshes[index];
            if (mesh.empty()) continue;
            
            double xd = cr.cx * 16.0 - x;
            double yd = cr.cy * 16.0 - cr.loadAnim - y;
            double zd = cr.cz * 16.0 - z;

            m[12] = float(xd);
            m[13] = float(yd);
            m[14] = float(zd);
            Renderer::shaders[Renderer::ActiveShader].setUniform("u_translation", m);
            mesh.render();
        }
    }
}

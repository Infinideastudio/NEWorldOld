#include "WorldRenderer.h"

namespace WorldRenderer {
    vector<RenderChunk> RenderChunkList;

    void ListRenderChunks(double x, double y, double z, int renderdistance, double curtime, std::optional<FrustumTest> frustum) {
        int cx = getchunkpos((int)x);
        int cy = getchunkpos((int)y);
        int cz = getchunkpos((int)z);

        RenderChunkList.clear();
        if (frustum) World::Chunk::setVisibilityBase(x, y, z, *frustum);
        for (auto const& [_, c]: World::chunks) {
            if (!c->meshed()) continue;
            if (World::chunkInRange(c->x(), c->y(), c->z(), cx, cy, cz, renderdistance)) {
                if (!frustum || c->visible()) {
                    RenderChunkList.push_back(RenderChunk(c.get(), float(curtime - lastupdate) * 30.0f));
                }
            }
        }
    }

    void RenderChunks(double x, double y, double z, int index) {
        assert(index >= 0 && index < 2);
        int TexcoordCount = MergeFace ? 3 : 2, ColorCount = 3;
        float m[16];
        memset(m, 0, sizeof(m));
        m[0] = m[5] = m[10] = m[15] = 1.0f;

        for (unsigned int i = 0; i < RenderChunkList.size(); i++) {
            RenderChunk const& cr = RenderChunkList[i];
            auto mesh = cr.meshes[index];
            if (mesh.first == 0 || mesh.second == 0) continue;
            glPushMatrix();
            glTranslated(cr.cx * 16.0 - x, cr.cy * 16.0 - cr.loadAnim - y, cr.cz * 16.0 - z);
            if (Renderer::AdvancedRender) {
                m[12] = cr.cx * 16.0f - (float)x;
                m[13] = cr.cy * 16.0f - (float)cr.loadAnim - (float)y;
                m[14] = cr.cz * 16.0f - (float)z;
                Renderer::shaders[Renderer::ActiveShader].setUniform("Translation", m);
                Renderer::renderbuffer(mesh.first, mesh.second, TexcoordCount, ColorCount, 3, 1);
            } else Renderer::renderbuffer(mesh.first, mesh.second, TexcoordCount, ColorCount);
            glPopMatrix();
        }

        glFlush();
    }
}

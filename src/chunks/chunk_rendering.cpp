module;

#include <algorithm>
#include <array>
#include <cassert>
#include <vector>
#include <glad/gl.h>

module chunks;
import types;
import vec3;
import blocks;
import rendering;
import textures;
import globals;

namespace chunks {

// One face in merge face
struct QuadPrimitive {
    int x = 0, y = 0, z = 0, length = 0, direction = 0;
    /*
     * If the vertexes have different colors (smoothed lighting), the primitive cannot connect with others.
     * This variable also means whether the vertexes have different colors.
     */
    bool once = false;
    // Vertex colors
    int col0 = 0, col1 = 0, col2 = 0, col3 = 0;
    // Block ID
    blocks::Id blk = blocks::Id(0);
    // Texture index
    TextureIndex tex = TextureIndex::NULLBLOCK;
};

class ChunkRenderData {
public:
    ChunkRenderData(Vec3i const& ccoord, std::array<Chunk const*, 3 * 3 * 3> const& neighbors) {
        auto index = 0uz;
        for (int x = -1; x < 17; x++) {
            auto rcx = x >> 4, bx = x & 0xF;
            for (int y = -1; y < 17; y++) {
                auto rcy = y >> 4, by = y & 0xF;
                for (int z = -1; z < 17; z++) {
                    auto rcz = z >> 4, bz = z & 0xF;
                    auto& block = _data[index++];
                    auto cptr = neighbors[((rcx + 1) * 3 + rcy + 1) * 3 + rcz + 1];
                    assert(cptr);
                    if (cptr == chunks::EMPTY_CHUNK) {
                        auto light = (ccoord.y + rcy < 0) ? NO_LIGHT : SKY_LIGHT;
                        block = blocks::BlockData{.id = base_blocks().air, .light = light};
                    } else {
                        block = cptr->block(Vec3u(bx, by, bz));
                    }
                    // Temporary: clamp to minimum light level 2
                    if (!block_info(block.id).opaque)
                        block.light = blocks::Light(block.light.sky(), std::max(block.light.block(), uint8_t{2}));
                }
            }
        }
    }

    auto block(int x, int y, int z) const -> blocks::BlockData const& {
        return _data[((x + 1) * 18 + y + 1) * 18 + z + 1];
    }

    // Temporary: mix two light levels.
    auto light(int x, int y, int z) const -> int {
        auto const& blk = block(x, y, z);
        return std::max(blk.light.sky(), blk.light.block());
    }

private:
    std::array<blocks::BlockData, 18 * 18 * 18> _data = {};
};

// Temporary: maximum value obtained after mixing two light levels.
constexpr auto MAX_LIGHT = 15.0f;

void RenderBlock(int x, int y, int z, ChunkRenderData const& rd) {
    auto bl = rd.block(x, y, z);
    auto neighbors = std::array{
        rd.block(x + 1, y, z),
        rd.block(x - 1, y, z),
        rd.block(x, y + 1, z),
        rd.block(x, y - 1, z),
        rd.block(x, y, z + 1),
        rd.block(x, y, z - 1),
    };
    auto tex = TextureIndex::NULLBLOCK;
    auto col1 = 0.0f, col2 = 0.0f, col3 = 0.0f, col4 = 0.0f;

    // Right face
    if (!(bl.id == base_blocks().air || bl.id == neighbors[0].id && bl.id != base_blocks().leaf
          || block_info(neighbors[0].id).opaque)) {
        if (NiceGrass && bl.id == base_blocks().grass && rd.block(x + 1, y - 1, z).id == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id, 0);
        else
            tex = Textures::getTextureIndex(bl.id, 1);
        col1 = col2 = col3 = col4 = rd.light(x + 1, y, z);
        if (SmoothLighting) {
            col1 =
                (col1 + rd.light(x + 1, y - 1, z) + rd.light(x + 1, y, z - 1) + rd.light(x + 1, y - 1, z - 1)) / 4.0f;
            col2 =
                (col2 + rd.light(x + 1, y + 1, z) + rd.light(x + 1, y, z - 1) + rd.light(x + 1, y + 1, z - 1)) / 4.0f;
            col3 =
                (col3 + rd.light(x + 1, y + 1, z) + rd.light(x + 1, y, z + 1) + rd.light(x + 1, y + 1, z + 1)) / 4.0f;
            col4 =
                (col4 + rd.light(x + 1, y - 1, z) + rd.light(x + 1, y, z + 1) + rd.light(x + 1, y - 1, z + 1)) / 4.0f;
        }
        col1 /= MAX_LIGHT, col2 /= MAX_LIGHT, col3 /= MAX_LIGHT, col4 /= MAX_LIGHT;
        if (!AdvancedRender)
            col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f, col4 *= 0.7f;
        Renderer::Attrib1f(static_cast<float>(bl.id.get()));
        Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
        Renderer::Normal3f(1.0f, 0.0f, 0.0f);
        Renderer::Color3f(col1, col1, col1);
        Renderer::TexCoord2f(1.0f, 0.0f);
        Renderer::Vertex3f(0.5f + x, -0.5f + y, -0.5f + z);
        Renderer::Color3f(col2, col2, col2);
        Renderer::TexCoord2f(1.0f, 1.0f);
        Renderer::Vertex3f(0.5f + x, 0.5f + y, -0.5f + z);
        Renderer::Color3f(col3, col3, col3);
        Renderer::TexCoord2f(0.0f, 1.0f);
        Renderer::Vertex3f(0.5f + x, 0.5f + y, 0.5f + z);
        Renderer::Color3f(col4, col4, col4);
        Renderer::TexCoord2f(0.0f, 0.0f);
        Renderer::Vertex3f(0.5f + x, -0.5f + y, 0.5f + z);
    }

    // Left Face
    if (!(bl.id == base_blocks().air || bl.id == neighbors[1].id && bl.id != base_blocks().leaf
          || block_info(neighbors[1].id).opaque)) {
        if (NiceGrass && bl.id == base_blocks().grass && rd.block(x - 1, y - 1, z).id == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id, 0);
        else
            tex = Textures::getTextureIndex(bl.id, 1);
        col1 = col2 = col3 = col4 = rd.light(x - 1, y, z);
        if (SmoothLighting) {
            col1 =
                (col1 + rd.light(x - 1, y - 1, z) + rd.light(x - 1, y, z - 1) + rd.light(x - 1, y - 1, z - 1)) / 4.0f;
            col2 =
                (col2 + rd.light(x - 1, y - 1, z) + rd.light(x - 1, y, z + 1) + rd.light(x - 1, y - 1, z + 1)) / 4.0f;
            col3 =
                (col3 + rd.light(x - 1, y + 1, z) + rd.light(x - 1, y, z + 1) + rd.light(x - 1, y + 1, z + 1)) / 4.0f;
            col4 =
                (col4 + rd.light(x - 1, y + 1, z) + rd.light(x - 1, y, z - 1) + rd.light(x - 1, y + 1, z - 1)) / 4.0f;
        }
        col1 /= MAX_LIGHT, col2 /= MAX_LIGHT, col3 /= MAX_LIGHT, col4 /= MAX_LIGHT;
        if (!AdvancedRender)
            col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f, col4 *= 0.7f;
        Renderer::Attrib1f(static_cast<float>(bl.id.get()));
        Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
        Renderer::Normal3f(-1.0f, 0.0f, 0.0f);
        Renderer::Color3f(col1, col1, col1);
        Renderer::TexCoord2f(0.0f, 0.0f);
        Renderer::Vertex3f(-0.5f + x, -0.5f + y, -0.5f + z);
        Renderer::Color3f(col2, col2, col2);
        Renderer::TexCoord2f(1.0f, 0.0f);
        Renderer::Vertex3f(-0.5f + x, -0.5f + y, 0.5f + z);
        Renderer::Color3f(col3, col3, col3);
        Renderer::TexCoord2f(1.0f, 1.0f);
        Renderer::Vertex3f(-0.5f + x, 0.5f + y, 0.5f + z);
        Renderer::Color3f(col4, col4, col4);
        Renderer::TexCoord2f(0.0f, 1.0f);
        Renderer::Vertex3f(-0.5f + x, 0.5f + y, -0.5f + z);
    }

    // Top Face
    if (!(bl.id == base_blocks().air || bl.id == neighbors[2].id && bl.id != base_blocks().leaf
          || block_info(neighbors[2].id).opaque)) {
        tex = Textures::getTextureIndex(bl.id, 0);
        col1 = col2 = col3 = col4 = rd.light(x, y + 1, z);
        if (SmoothLighting) {
            col1 =
                (col1 + rd.light(x, y + 1, z - 1) + rd.light(x - 1, y + 1, z) + rd.light(x - 1, y + 1, z - 1)) / 4.0f;
            col2 =
                (col2 + rd.light(x, y + 1, z + 1) + rd.light(x - 1, y + 1, z) + rd.light(x - 1, y + 1, z + 1)) / 4.0f;
            col3 =
                (col3 + rd.light(x, y + 1, z + 1) + rd.light(x + 1, y + 1, z) + rd.light(x + 1, y + 1, z + 1)) / 4.0f;
            col4 =
                (col4 + rd.light(x, y + 1, z - 1) + rd.light(x + 1, y + 1, z) + rd.light(x + 1, y + 1, z - 1)) / 4.0f;
        }
        col1 /= MAX_LIGHT, col2 /= MAX_LIGHT, col3 /= MAX_LIGHT, col4 /= MAX_LIGHT;
        Renderer::Attrib1f(static_cast<float>(bl.id.get()));
        Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
        Renderer::Normal3f(0.0f, 1.0f, 0.0f);
        Renderer::Color3f(col1, col1, col1);
        Renderer::TexCoord2f(0.0f, 1.0f);
        Renderer::Vertex3f(-0.5f + x, 0.5f + y, -0.5f + z);
        Renderer::Color3f(col2, col2, col2);
        Renderer::TexCoord2f(0.0f, 0.0f);
        Renderer::Vertex3f(-0.5f + x, 0.5f + y, 0.5f + z);
        Renderer::Color3f(col3, col3, col3);
        Renderer::TexCoord2f(1.0f, 0.0f);
        Renderer::Vertex3f(0.5f + x, 0.5f + y, 0.5f + z);
        Renderer::Color3f(col4, col4, col4);
        Renderer::TexCoord2f(1.0f, 1.0f);
        Renderer::Vertex3f(0.5f + x, 0.5f + y, -0.5f + z);
    }

    // Bottom Face
    if (!(bl.id == base_blocks().air || bl.id == neighbors[3].id && bl.id != base_blocks().leaf
          || block_info(neighbors[3].id).opaque)) {
        tex = Textures::getTextureIndex(bl.id, 2);
        col1 = col2 = col3 = col4 = rd.light(x, y - 1, z);
        if (SmoothLighting) {
            col1 =
                (col1 + rd.light(x, y - 1, z - 1) + rd.light(x - 1, y - 1, z) + rd.light(x - 1, y - 1, z - 1)) / 4.0f;
            col2 =
                (col2 + rd.light(x, y - 1, z - 1) + rd.light(x + 1, y - 1, z) + rd.light(x + 1, y - 1, z - 1)) / 4.0f;
            col3 =
                (col3 + rd.light(x, y - 1, z + 1) + rd.light(x + 1, y - 1, z) + rd.light(x + 1, y - 1, z + 1)) / 4.0f;
            col4 =
                (col4 + rd.light(x, y - 1, z + 1) + rd.light(x - 1, y - 1, z) + rd.light(x - 1, y - 1, z + 1)) / 4.0f;
        }
        col1 /= MAX_LIGHT, col2 /= MAX_LIGHT, col3 /= MAX_LIGHT, col4 /= MAX_LIGHT;
        Renderer::Attrib1f(static_cast<float>(bl.id.get()));
        Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
        Renderer::Normal3f(0.0f, -1.0f, 0.0f);
        Renderer::Color3f(col1, col1, col1);
        Renderer::TexCoord2f(1.0f, 1.0f);
        Renderer::Vertex3f(-0.5f + x, -0.5f + y, -0.5f + z);
        Renderer::Color3f(col2, col2, col2);
        Renderer::TexCoord2f(0.0f, 1.0f);
        Renderer::Vertex3f(0.5f + x, -0.5f + y, -0.5f + z);
        Renderer::Color3f(col3, col3, col3);
        Renderer::TexCoord2f(0.0f, 0.0f);
        Renderer::Vertex3f(0.5f + x, -0.5f + y, 0.5f + z);
        Renderer::Color3f(col4, col4, col4);
        Renderer::TexCoord2f(1.0f, 0.0f);
        Renderer::Vertex3f(-0.5f + x, -0.5f + y, 0.5f + z);
    }

    // Front Face
    if (!(bl.id == base_blocks().air || bl.id == neighbors[4].id && bl.id != base_blocks().leaf
          || block_info(neighbors[4].id).opaque)) {
        if (NiceGrass && bl.id == base_blocks().grass && rd.block(x, y - 1, z + 1).id == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id, 0);
        else
            tex = Textures::getTextureIndex(bl.id, 1);
        col1 = col2 = col3 = col4 = rd.light(x, y, z + 1);
        if (SmoothLighting) {
            col1 =
                (col1 + rd.light(x, y - 1, z + 1) + rd.light(x - 1, y, z + 1) + rd.light(x - 1, y - 1, z + 1)) / 4.0f;
            col2 =
                (col2 + rd.light(x, y - 1, z + 1) + rd.light(x + 1, y, z + 1) + rd.light(x + 1, y - 1, z + 1)) / 4.0f;
            col3 =
                (col3 + rd.light(x, y + 1, z + 1) + rd.light(x + 1, y, z + 1) + rd.light(x + 1, y + 1, z + 1)) / 4.0f;
            col4 =
                (col4 + rd.light(x, y + 1, z + 1) + rd.light(x - 1, y, z + 1) + rd.light(x - 1, y + 1, z + 1)) / 4.0f;
        }
        col1 /= MAX_LIGHT, col2 /= MAX_LIGHT, col3 /= MAX_LIGHT, col4 /= MAX_LIGHT;
        if (!AdvancedRender)
            col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f, col4 *= 0.5f;
        Renderer::Attrib1f(static_cast<float>(bl.id.get()));
        Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
        Renderer::Normal3f(0.0f, 0.0f, 1.0f);
        Renderer::Color3f(col1, col1, col1);
        Renderer::TexCoord2f(0.0f, 0.0f);
        Renderer::Vertex3f(-0.5f + x, -0.5f + y, 0.5f + z);
        Renderer::Color3f(col2, col2, col2);
        Renderer::TexCoord2f(1.0f, 0.0f);
        Renderer::Vertex3f(0.5f + x, -0.5f + y, 0.5f + z);
        Renderer::Color3f(col3, col3, col3);
        Renderer::TexCoord2f(1.0f, 1.0f);
        Renderer::Vertex3f(0.5f + x, 0.5f + y, 0.5f + z);
        Renderer::Color3f(col4, col4, col4);
        Renderer::TexCoord2f(0.0f, 1.0f);
        Renderer::Vertex3f(-0.5f + x, 0.5f + y, 0.5f + z);
    }

    // Back Face
    if (!(bl.id == base_blocks().air || bl.id == neighbors[5].id && bl.id != base_blocks().leaf
          || block_info(neighbors[5].id).opaque)) {
        if (NiceGrass && bl.id == base_blocks().grass && rd.block(x, y - 1, z - 1).id == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id, 0);
        else
            tex = Textures::getTextureIndex(bl.id, 1);
        col1 = col2 = col3 = col4 = rd.light(x, y, z - 1);
        if (SmoothLighting) {
            col1 =
                (col1 + rd.light(x, y - 1, z - 1) + rd.light(x - 1, y, z - 1) + rd.light(x - 1, y - 1, z - 1)) / 4.0f;
            col2 =
                (col2 + rd.light(x, y + 1, z - 1) + rd.light(x - 1, y, z - 1) + rd.light(x - 1, y + 1, z - 1)) / 4.0f;
            col3 =
                (col3 + rd.light(x, y + 1, z - 1) + rd.light(x + 1, y, z - 1) + rd.light(x + 1, y + 1, z - 1)) / 4.0f;
            col4 =
                (col4 + rd.light(x, y - 1, z - 1) + rd.light(x + 1, y, z - 1) + rd.light(x + 1, y - 1, z - 1)) / 4.0f;
        }
        col1 /= MAX_LIGHT, col2 /= MAX_LIGHT, col3 /= MAX_LIGHT, col4 /= MAX_LIGHT;
        if (!AdvancedRender)
            col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f, col4 *= 0.5f;
        Renderer::Attrib1f(static_cast<float>(bl.id.get()));
        Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
        Renderer::Normal3f(0.0f, 0.0f, -1.0f);
        Renderer::Color3f(col1, col1, col1);
        Renderer::TexCoord2f(1.0f, 0.0f);
        Renderer::Vertex3f(-0.5f + x, -0.5f + y, -0.5f + z);
        Renderer::Color3f(col2, col2, col2);
        Renderer::TexCoord2f(1.0f, 1.0f);
        Renderer::Vertex3f(-0.5f + x, 0.5f + y, -0.5f + z);
        Renderer::Color3f(col3, col3, col3);
        Renderer::TexCoord2f(0.0f, 1.0f);
        Renderer::Vertex3f(0.5f + x, 0.5f + y, -0.5f + z);
        Renderer::Color3f(col4, col4, col4);
        Renderer::TexCoord2f(0.0f, 0.0f);
        Renderer::Vertex3f(0.5f + x, -0.5f + y, -0.5f + z);
    }
}

/*
The vertex order of merge face render
Numbered from 0 to 3:

(k++)
...
|    |
+----+--
|    |
|    |    |
3----2----+-
|curr|    |   ...
|face|    |   (j++)
0----1----+--
*/

void RenderPrimitive(QuadPrimitive const& p) {
    float col0 = (float) p.col0 * 0.25f / MAX_LIGHT;
    float col1 = (float) p.col1 * 0.25f / MAX_LIGHT;
    float col2 = (float) p.col2 * 0.25f / MAX_LIGHT;
    float col3 = (float) p.col3 * 0.25f / MAX_LIGHT;
    int x = p.x, y = p.y, z = p.z, length = p.length;

    Renderer::Attrib1f(static_cast<float>(p.blk.get()));
    Renderer::TexCoord3f(0.0f, 0.0f, static_cast<float>(p.tex));

    switch (p.direction) {
        case 0:
            if (!AdvancedRender)
                col0 *= 0.7f, col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f;
            Renderer::Normal3f(1.0f, 0.0f, 0.0f);
            Renderer::Color3f(col0, col0, col0);
            Renderer::TexCoord2f(0.0f, 0.0f);
            Renderer::Vertex3f(x + 0.5f, y - 0.5f, z - 0.5f);
            Renderer::Color3f(col1, col1, col1);
            Renderer::TexCoord2f(0.0f, 1.0f);
            Renderer::Vertex3f(x + 0.5f, y + 0.5f, z - 0.5f);
            Renderer::Color3f(col2, col2, col2);
            Renderer::TexCoord2f(length + 1.0f, 1.0f);
            Renderer::Vertex3f(x + 0.5f, y + 0.5f, z + length + 0.5f);
            Renderer::Color3f(col3, col3, col3);
            Renderer::TexCoord2f(length + 1.0f, 0.0f);
            Renderer::Vertex3f(x + 0.5f, y - 0.5f, z + length + 0.5f);
            break;
        case 1:
            if (!AdvancedRender)
                col0 *= 0.7f, col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f;
            Renderer::Normal3f(-1.0f, 0.0f, 0.0f);
            Renderer::Color3f(col0, col0, col0);
            Renderer::TexCoord2f(0.0f, 1.0f);
            Renderer::Vertex3f(x - 0.5f, y + 0.5f, z - 0.5f);
            Renderer::Color3f(col1, col1, col1);
            Renderer::TexCoord2f(0.0f, 0.0f);
            Renderer::Vertex3f(x - 0.5f, y - 0.5f, z - 0.5f);
            Renderer::Color3f(col2, col2, col2);
            Renderer::TexCoord2f(length + 1.0f, 0.0f);
            Renderer::Vertex3f(x - 0.5f, y - 0.5f, z + length + 0.5f);
            Renderer::Color3f(col3, col3, col3);
            Renderer::TexCoord2f(length + 1.0f, 1.0f);
            Renderer::Vertex3f(x - 0.5f, y + 0.5f, z + length + 0.5f);
            break;
        case 2:
            Renderer::Normal3f(0.0f, 1.0f, 0.0f);
            Renderer::Color3f(col0, col0, col0);
            Renderer::TexCoord2f(0.0f, 0.0f);
            Renderer::Vertex3f(x + 0.5f, y + 0.5f, z - 0.5f);
            Renderer::Color3f(col1, col1, col1);
            Renderer::TexCoord2f(0.0f, 1.0f);
            Renderer::Vertex3f(x - 0.5f, y + 0.5f, z - 0.5f);
            Renderer::Color3f(col2, col2, col2);
            Renderer::TexCoord2f(length + 1.0f, 1.0f);
            Renderer::Vertex3f(x - 0.5f, y + 0.5f, z + length + 0.5f);
            Renderer::Color3f(col3, col3, col3);
            Renderer::TexCoord2f(length + 1.0f, 0.0f);
            Renderer::Vertex3f(x + 0.5f, y + 0.5f, z + length + 0.5f);
            break;
        case 3:
            Renderer::Normal3f(0.0f, -1.0f, 0.0f);
            Renderer::Color3f(col0, col0, col0);
            Renderer::TexCoord2f(0.0f, 0.0f);
            Renderer::Vertex3f(x - 0.5f, y - 0.5f, z - 0.5f);
            Renderer::Color3f(col1, col1, col1);
            Renderer::TexCoord2f(0.0f, 1.0f);
            Renderer::Vertex3f(x + 0.5f, y - 0.5f, z - 0.5f);
            Renderer::Color3f(col2, col2, col2);
            Renderer::TexCoord2f(length + 1.0f, 1.0f);
            Renderer::Vertex3f(x + 0.5f, y - 0.5f, z + length + 0.5f);
            Renderer::Color3f(col3, col3, col3);
            Renderer::TexCoord2f(length + 1.0f, 0.0f);
            Renderer::Vertex3f(x - 0.5f, y - 0.5f, z + length + 0.5f);
            break;
        case 4:
            if (!AdvancedRender)
                col0 *= 0.5f, col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f;
            Renderer::Normal3f(0.0f, 0.0f, 1.0f);
            Renderer::Color3f(col0, col0, col0);
            Renderer::TexCoord2f(0.0f, 1.0f);
            Renderer::Vertex3f(x - 0.5f, y + 0.5f, z + 0.5f);
            Renderer::Color3f(col1, col1, col1);
            Renderer::TexCoord2f(0.0f, 0.0f);
            Renderer::Vertex3f(x - 0.5f, y - 0.5f, z + 0.5f);
            Renderer::Color3f(col2, col2, col2);
            Renderer::TexCoord2f(length + 1.0f, 0.0f);
            Renderer::Vertex3f(x + length + 0.5f, y - 0.5f, z + 0.5f);
            Renderer::Color3f(col3, col3, col3);
            Renderer::TexCoord2f(length + 1.0f, 1.0f);
            Renderer::Vertex3f(x + length + 0.5f, y + 0.5f, z + 0.5f);
            break;
        case 5:
            if (!AdvancedRender)
                col0 *= 0.5f, col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f;
            Renderer::Normal3f(0.0f, 0.0f, -1.0f);
            Renderer::Color3f(col0, col0, col0);
            Renderer::TexCoord2f(0.0f, 0.0f);
            Renderer::Vertex3f(x - 0.5f, y - 0.5f, z - 0.5f);
            Renderer::Color3f(col1, col1, col1);
            Renderer::TexCoord2f(0.0f, 1.0f);
            Renderer::Vertex3f(x - 0.5f, y + 0.5f, z - 0.5f);
            Renderer::Color3f(col2, col2, col2);
            Renderer::TexCoord2f(length + 1.0f, 1.0f);
            Renderer::Vertex3f(x + length + 0.5f, y + 0.5f, z - 0.5f);
            Renderer::Color3f(col3, col3, col3);
            Renderer::TexCoord2f(length + 1.0f, 0.0f);
            Renderer::Vertex3f(x + length + 0.5f, y - 0.5f, z - 0.5f);
            break;
    }
}

auto _render_chunk(Vec3i const& ccoord, std::array<Chunk const*, 3 * 3 * 3> neighbors)
    -> std::vector<Renderer::VertexBuffer> {
    auto res = std::vector<Renderer::VertexBuffer>();
    auto rd = ChunkRenderData(ccoord, neighbors);
    for (size_t steps = 0; steps < 2; steps++) {
        if (AdvancedRender)
            Renderer::Begin(GL_QUADS, 3, 3, 1, 3, 1);
        else
            Renderer::Begin(GL_QUADS, 3, 3, 1);
        for (int x = 0; x < 16; x++)
            for (int y = 0; y < 16; y++)
                for (int z = 0; z < 16; z++) {
                    auto const& info = block_info(rd.block(x, y, z).id);
                    if (steps == 0 && !info.translucent || steps == 1 && info.translucent) {
                        RenderBlock(x, y, z, rd);
                    }
                }
        res.emplace_back(Renderer::End(true));
    }
    return res;
}

auto _merge_face_render_chunk(Vec3i const& ccoord, std::array<Chunk const*, 3 * 3 * 3> neighbors)
    -> std::vector<Renderer::VertexBuffer> {
    auto res = std::vector<Renderer::VertexBuffer>();
    auto rd = ChunkRenderData(ccoord, neighbors);
    for (size_t steps = 0; steps < 2; steps++) {
        if (AdvancedRender)
            Renderer::Begin(GL_QUADS, 3, 3, 1, 3, 1);
        else
            Renderer::Begin(GL_QUADS, 3, 3, 1);
        for (int d = 0; d < 6; d++) {
            // Render current face
            for (int i = 0; i < 16; i++)
                for (int j = 0; j < 16; j++) {
                    QuadPrimitive cur;
                    bool valid = false;
                    // Linear merge
                    for (int k = 0; k < 16; k++) {
                        // Get position
                        int x = 0, y = 0, z = 0;
                        int dx = 0, dy = 0, dz = 0;
                        switch (d) {
                            case 0:
                                x = i, y = j, z = k, dx = 1;
                                break;
                            case 1:
                                x = i, y = j, z = k, dx = -1;
                                break;
                            case 2:
                                x = j, y = i, z = k, dy = 1;
                                break;
                            case 3:
                                x = j, y = i, z = k, dy = -1;
                                break;
                            case 4:
                                x = k, y = j, z = i, dz = 1;
                                break;
                            case 5:
                                x = k, y = j, z = i, dz = -1;
                                break;
                        }
                        // Get block ID
                        auto bl = rd.block(x, y, z);
                        auto neighbour = rd.block(x + dx, y + dy, z + dz);
                        auto const& info = block_info(bl.id);
                        if (bl.id == base_blocks().air || bl.id == neighbour.id && bl.id != base_blocks().leaf
                            || block_info(neighbour.id).opaque
                            || !(steps == 0 && !info.translucent || steps == 1 && info.translucent)) {
                            // Not valid block
                            if (valid) {
                                RenderPrimitive(cur);
                                valid = false;
                            }
                            continue;
                        }
                        // Get texture and brightness
                        size_t face = 0;
                        if (d == 2)
                            face = 0;
                        else if (d == 3)
                            face = 2;
                        else
                            face = 1;
                        TextureIndex tex = Textures::getTextureIndex(bl.id, face);
                        int br = 0, col0 = 0, col1 = 0, col2 = 0, col3 = 0;
                        switch (d) {
                            case 0:
                                if (NiceGrass && bl.id == base_blocks().grass
                                    && rd.block(x + 1, y - 1, z).id == base_blocks().grass)
                                    tex = Textures::getTextureIndex(bl.id, 0);
                                br = rd.light(x + 1, y, z);
                                if (SmoothLighting) {
                                    col0 = br + rd.light(x + 1, y - 1, z) + rd.light(x + 1, y, z - 1)
                                         + rd.light(x + 1, y - 1, z - 1);
                                    col1 = br + rd.light(x + 1, y + 1, z) + rd.light(x + 1, y, z - 1)
                                         + rd.light(x + 1, y + 1, z - 1);
                                    col2 = br + rd.light(x + 1, y + 1, z) + rd.light(x + 1, y, z + 1)
                                         + rd.light(x + 1, y + 1, z + 1);
                                    col3 = br + rd.light(x + 1, y - 1, z) + rd.light(x + 1, y, z + 1)
                                         + rd.light(x + 1, y - 1, z + 1);
                                } else
                                    col0 = col1 = col2 = col3 = br * 4;
                                break;
                            case 1:
                                if (NiceGrass && bl.id == base_blocks().grass
                                    && rd.block(x - 1, y - 1, z).id == base_blocks().grass)
                                    tex = Textures::getTextureIndex(bl.id, 0);
                                br = rd.light(x - 1, y, z);
                                if (SmoothLighting) {
                                    col0 = br + rd.light(x - 1, y + 1, z) + rd.light(x - 1, y, z - 1)
                                         + rd.light(x - 1, y + 1, z - 1);
                                    col1 = br + rd.light(x - 1, y - 1, z) + rd.light(x - 1, y, z - 1)
                                         + rd.light(x - 1, y - 1, z - 1);
                                    col2 = br + rd.light(x - 1, y - 1, z) + rd.light(x - 1, y, z + 1)
                                         + rd.light(x - 1, y - 1, z + 1);
                                    col3 = br + rd.light(x - 1, y + 1, z) + rd.light(x - 1, y, z + 1)
                                         + rd.light(x - 1, y + 1, z + 1);
                                } else
                                    col0 = col1 = col2 = col3 = br * 4;
                                break;
                            case 2:
                                br = rd.light(x, y + 1, z);
                                if (SmoothLighting) {
                                    col0 = br + rd.light(x + 1, y + 1, z) + rd.light(x, y + 1, z - 1)
                                         + rd.light(x + 1, y + 1, z - 1);
                                    col1 = br + rd.light(x - 1, y + 1, z) + rd.light(x, y + 1, z - 1)
                                         + rd.light(x - 1, y + 1, z - 1);
                                    col2 = br + rd.light(x - 1, y + 1, z) + rd.light(x, y + 1, z + 1)
                                         + rd.light(x - 1, y + 1, z + 1);
                                    col3 = br + rd.light(x + 1, y + 1, z) + rd.light(x, y + 1, z + 1)
                                         + rd.light(x + 1, y + 1, z + 1);
                                } else
                                    col0 = col1 = col2 = col3 = br * 4;
                                break;
                            case 3:
                                br = rd.light(x, y - 1, z);
                                if (SmoothLighting) {
                                    col0 = br + rd.light(x - 1, y - 1, z) + rd.light(x, y - 1, z - 1)
                                         + rd.light(x - 1, y - 1, z - 1);
                                    col1 = br + rd.light(x + 1, y - 1, z) + rd.light(x, y - 1, z - 1)
                                         + rd.light(x + 1, y - 1, z - 1);
                                    col2 = br + rd.light(x + 1, y - 1, z) + rd.light(x, y - 1, z + 1)
                                         + rd.light(x + 1, y - 1, z + 1);
                                    col3 = br + rd.light(x - 1, y - 1, z) + rd.light(x, y - 1, z + 1)
                                         + rd.light(x - 1, y - 1, z + 1);
                                } else
                                    col0 = col1 = col2 = col3 = br * 4;
                                break;
                            case 4:
                                if (NiceGrass && bl.id == base_blocks().grass
                                    && rd.block(x, y - 1, z + 1).id == base_blocks().grass)
                                    tex = Textures::getTextureIndex(bl.id, 0);
                                br = rd.light(x, y, z + 1);
                                if (SmoothLighting) {
                                    col0 = br + rd.light(x - 1, y, z + 1) + rd.light(x, y + 1, z + 1)
                                         + rd.light(x - 1, y + 1, z + 1);
                                    col1 = br + rd.light(x - 1, y, z + 1) + rd.light(x, y - 1, z + 1)
                                         + rd.light(x - 1, y - 1, z + 1);
                                    col2 = br + rd.light(x + 1, y, z + 1) + rd.light(x, y - 1, z + 1)
                                         + rd.light(x + 1, y - 1, z + 1);
                                    col3 = br + rd.light(x + 1, y, z + 1) + rd.light(x, y + 1, z + 1)
                                         + rd.light(x + 1, y + 1, z + 1);
                                } else
                                    col0 = col1 = col2 = col3 = br * 4;
                                break;
                            case 5:
                                if (NiceGrass && bl.id == base_blocks().grass
                                    && rd.block(x, y - 1, z - 1).id == base_blocks().grass)
                                    tex = Textures::getTextureIndex(bl.id, 0);
                                br = rd.light(x, y, z - 1);
                                if (SmoothLighting) {
                                    col0 = br + rd.light(x - 1, y, z - 1) + rd.light(x, y - 1, z - 1)
                                         + rd.light(x - 1, y - 1, z - 1);
                                    col1 = br + rd.light(x - 1, y, z - 1) + rd.light(x, y + 1, z - 1)
                                         + rd.light(x - 1, y + 1, z - 1);
                                    col2 = br + rd.light(x + 1, y, z - 1) + rd.light(x, y + 1, z - 1)
                                         + rd.light(x + 1, y + 1, z - 1);
                                    col3 = br + rd.light(x + 1, y, z - 1) + rd.light(x, y - 1, z - 1)
                                         + rd.light(x + 1, y - 1, z - 1);
                                } else
                                    col0 = col1 = col2 = col3 = br * 4;
                                break;
                        }
                        // Render
                        bool once = col0 != col1 || col1 != col2 || col2 != col3;
                        if (valid) {
                            if (once || cur.once || bl.id != cur.blk || tex != cur.tex || col0 != cur.col0) {
                                RenderPrimitive(cur);
                                cur.x = x;
                                cur.y = y;
                                cur.z = z;
                                cur.length = 0;
                                cur.direction = d;
                                cur.once = once;
                                cur.blk = bl.id;
                                cur.tex = tex;
                                cur.col0 = col0;
                                cur.col1 = col1;
                                cur.col2 = col2;
                                cur.col3 = col3;
                            } else
                                cur.length++;
                        } else {
                            valid = true;
                            cur.x = x;
                            cur.y = y;
                            cur.z = z;
                            cur.length = 0;
                            cur.direction = d;
                            cur.once = once;
                            cur.blk = bl.id;
                            cur.tex = tex;
                            cur.col0 = col0;
                            cur.col1 = col1;
                            cur.col2 = col2;
                            cur.col3 = col3;
                        }
                    }
                    if (valid) {
                        RenderPrimitive(cur);
                        valid = false;
                    }
                }
        }
        res.emplace_back(Renderer::End(true));
    }
    return res;
}

void Chunk::build_meshes(std::array<Chunk const*, 3 * 3 * 3> const& neighbors) {
    // Build new VBOs
    _meshes = MergeFace ? _merge_face_render_chunk(_coord, neighbors) : _render_chunk(_coord, neighbors);

    // Update flags
    if (!_meshed)
        _load_anim = static_cast<float>(_coord.y) * 16.0f + 16.0f;
    _meshed = true;
    _updated = false;

    meshed_chunks++;
    updated_chunks++;
}

}

module;

#include <array>
#include <vector>
#include <glad/gl.h>

module chunks;
import rendering;
import blocks;
import types;
import textures;
import globals;
import worlds;
import vec3;
import globals;

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
    BlockID blk = BlockID::AIR;
    // Texture index
    TextureIndex tex = TextureIndex::NULLBLOCK;
};

class ChunkRenderData {
public:
    std::array<BlockID, 18 * 18 * 18> blocks;
    std::array<Brightness, 18 * 18 * 18> brightness;

    ChunkRenderData(std::array<Chunk const*, 27> const& neighbors) {
        auto ccoord = neighbors[1 * 3 * 3 + 1 * 3 + 1]->coord();
        Chunk const* cptr[3][3][3] = {};

        for (int x = -1; x <= 1; x++)
            for (int y = -1; y <= 1; y++)
                for (int z = -1; z <= 1; z++)
                    cptr[x + 1][y + 1][z + 1] = neighbors[((x + 1) * 3 + (y + 1)) * 3 + (z + 1)];

        int index = 0;
        for (int x = -1; x < 17; x++) {
            int rcx = 0, bx = x;
            if (x < 0)
                rcx--, bx += 16;
            else if (x >= 16)
                rcx++, bx -= 16;

            for (int y = -1; y < 17; y++) {
                int rcy = 0, by = y;
                if (y < 0)
                    rcy--, by += 16;
                else if (y >= 16)
                    rcy++, by -= 16;

                for (int z = -1; z < 17; z++) {
                    int rcz = 0, bz = z;
                    if (z < 0)
                        rcz--, bz += 16;
                    else if (z >= 16)
                        rcz++, bz -= 16;

                    auto p = cptr[rcx + 1][rcy + 1][rcz + 1];
                    if (p == nullptr) {
                        blocks[index] = BlockID::ROCK;
                        brightness[index] = SkyBrightness;
                    } else if (p == EmptyChunkPtr) {
                        blocks[index] = BlockID::AIR;
                        brightness[index] = (ccoord.y + rcy < 0) ? MinBrightness : SkyBrightness;
                    } else {
                        blocks[index] = p->getBlock(bx, by, bz);
                        brightness[index] = p->getBrightness(bx, by, bz);
                    }
                    index++;
                }
            }
        }
    }

    BlockID getBlock(int x, int y, int z) const {
        return blocks[(x + 1) * 18 * 18 + (y + 1) * 18 + (z + 1)];
    }

    Brightness getBrightness(int x, int y, int z) const {
        return brightness[(x + 1) * 18 * 18 + (y + 1) * 18 + (z + 1)];
    }
};

void RenderBlock(int x, int y, int z, ChunkRenderData const& rd) {
    BlockID bl = rd.getBlock(x, y, z);
    BlockID neighbors[6] = {
        rd.getBlock(x + 1, y, z),
        rd.getBlock(x - 1, y, z),
        rd.getBlock(x, y + 1, z),
        rd.getBlock(x, y - 1, z),
        rd.getBlock(x, y, z + 1),
        rd.getBlock(x, y, z - 1),
    };
    TextureIndex tex;
    float col1, col2, col3, col4;

    // Right face
    if (!(bl == BlockID::AIR || bl == neighbors[0] && bl != BlockID::LEAF || BlockInfo(neighbors[0]).isOpaque())) {
        if (NiceGrass && bl == BlockID::GRASS && rd.getBlock(x + 1, y - 1, z) == BlockID::GRASS)
            tex = Textures::getTextureIndex(bl, 0);
        else
            tex = Textures::getTextureIndex(bl, 1);
        col1 = col2 = col3 = col4 = rd.getBrightness(x + 1, y, z);
        if (SmoothLighting) {
            col1 = (col1 + rd.getBrightness(x + 1, y - 1, z) + rd.getBrightness(x + 1, y, z - 1)
                    + rd.getBrightness(x + 1, y - 1, z - 1))
                 / 4.0f;
            col2 = (col2 + rd.getBrightness(x + 1, y + 1, z) + rd.getBrightness(x + 1, y, z - 1)
                    + rd.getBrightness(x + 1, y + 1, z - 1))
                 / 4.0f;
            col3 = (col3 + rd.getBrightness(x + 1, y + 1, z) + rd.getBrightness(x + 1, y, z + 1)
                    + rd.getBrightness(x + 1, y + 1, z + 1))
                 / 4.0f;
            col4 = (col4 + rd.getBrightness(x + 1, y - 1, z) + rd.getBrightness(x + 1, y, z + 1)
                    + rd.getBrightness(x + 1, y - 1, z + 1))
                 / 4.0f;
        }
        col1 /= MaxBrightness, col2 /= MaxBrightness, col3 /= MaxBrightness, col4 /= MaxBrightness;
        if (!AdvancedRender)
            col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f, col4 *= 0.7f;
        Renderer::Attrib1f(static_cast<float>(bl));
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
    if (!(bl == BlockID::AIR || bl == neighbors[1] && bl != BlockID::LEAF || BlockInfo(neighbors[1]).isOpaque())) {
        if (NiceGrass && bl == BlockID::GRASS && rd.getBlock(x - 1, y - 1, z) == BlockID::GRASS)
            tex = Textures::getTextureIndex(bl, 0);
        else
            tex = Textures::getTextureIndex(bl, 1);
        col1 = col2 = col3 = col4 = rd.getBrightness(x - 1, y, z);
        if (SmoothLighting) {
            col1 = (col1 + rd.getBrightness(x - 1, y - 1, z) + rd.getBrightness(x - 1, y, z - 1)
                    + rd.getBrightness(x - 1, y - 1, z - 1))
                 / 4.0f;
            col2 = (col2 + rd.getBrightness(x - 1, y - 1, z) + rd.getBrightness(x - 1, y, z + 1)
                    + rd.getBrightness(x - 1, y - 1, z + 1))
                 / 4.0f;
            col3 = (col3 + rd.getBrightness(x - 1, y + 1, z) + rd.getBrightness(x - 1, y, z + 1)
                    + rd.getBrightness(x - 1, y + 1, z + 1))
                 / 4.0f;
            col4 = (col4 + rd.getBrightness(x - 1, y + 1, z) + rd.getBrightness(x - 1, y, z - 1)
                    + rd.getBrightness(x - 1, y + 1, z - 1))
                 / 4.0f;
        }
        col1 /= MaxBrightness, col2 /= MaxBrightness, col3 /= MaxBrightness, col4 /= MaxBrightness;
        if (!AdvancedRender)
            col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f, col4 *= 0.7f;
        Renderer::Attrib1f(static_cast<float>(bl));
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
    if (!(bl == BlockID::AIR || bl == neighbors[2] && bl != BlockID::LEAF || BlockInfo(neighbors[2]).isOpaque())) {
        tex = Textures::getTextureIndex(bl, 0);
        col1 = col2 = col3 = col4 = rd.getBrightness(x, y + 1, z);
        if (SmoothLighting) {
            col1 = (col1 + rd.getBrightness(x, y + 1, z - 1) + rd.getBrightness(x - 1, y + 1, z)
                    + rd.getBrightness(x - 1, y + 1, z - 1))
                 / 4.0f;
            col2 = (col2 + rd.getBrightness(x, y + 1, z + 1) + rd.getBrightness(x - 1, y + 1, z)
                    + rd.getBrightness(x - 1, y + 1, z + 1))
                 / 4.0f;
            col3 = (col3 + rd.getBrightness(x, y + 1, z + 1) + rd.getBrightness(x + 1, y + 1, z)
                    + rd.getBrightness(x + 1, y + 1, z + 1))
                 / 4.0f;
            col4 = (col4 + rd.getBrightness(x, y + 1, z - 1) + rd.getBrightness(x + 1, y + 1, z)
                    + rd.getBrightness(x + 1, y + 1, z - 1))
                 / 4.0f;
        }
        col1 /= MaxBrightness, col2 /= MaxBrightness, col3 /= MaxBrightness, col4 /= MaxBrightness;
        Renderer::Attrib1f(static_cast<float>(bl));
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
    if (!(bl == BlockID::AIR || bl == neighbors[3] && bl != BlockID::LEAF || BlockInfo(neighbors[3]).isOpaque())) {
        tex = Textures::getTextureIndex(bl, 2);
        col1 = col2 = col3 = col4 = rd.getBrightness(x, y - 1, z);
        if (SmoothLighting) {
            col1 = (col1 + rd.getBrightness(x, y - 1, z - 1) + rd.getBrightness(x - 1, y - 1, z)
                    + rd.getBrightness(x - 1, y - 1, z - 1))
                 / 4.0f;
            col2 = (col2 + rd.getBrightness(x, y - 1, z - 1) + rd.getBrightness(x + 1, y - 1, z)
                    + rd.getBrightness(x + 1, y - 1, z - 1))
                 / 4.0f;
            col3 = (col3 + rd.getBrightness(x, y - 1, z + 1) + rd.getBrightness(x + 1, y - 1, z)
                    + rd.getBrightness(x + 1, y - 1, z + 1))
                 / 4.0f;
            col4 = (col4 + rd.getBrightness(x, y - 1, z + 1) + rd.getBrightness(x - 1, y - 1, z)
                    + rd.getBrightness(x - 1, y - 1, z + 1))
                 / 4.0f;
        }
        col1 /= MaxBrightness, col2 /= MaxBrightness, col3 /= MaxBrightness, col4 /= MaxBrightness;
        Renderer::Attrib1f(static_cast<float>(bl));
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
    if (!(bl == BlockID::AIR || bl == neighbors[4] && bl != BlockID::LEAF || BlockInfo(neighbors[4]).isOpaque())) {
        if (NiceGrass && bl == BlockID::GRASS && rd.getBlock(x, y - 1, z + 1) == BlockID::GRASS)
            tex = Textures::getTextureIndex(bl, 0);
        else
            tex = Textures::getTextureIndex(bl, 1);
        col1 = col2 = col3 = col4 = rd.getBrightness(x, y, z + 1);
        if (SmoothLighting) {
            col1 = (col1 + rd.getBrightness(x, y - 1, z + 1) + rd.getBrightness(x - 1, y, z + 1)
                    + rd.getBrightness(x - 1, y - 1, z + 1))
                 / 4.0f;
            col2 = (col2 + rd.getBrightness(x, y - 1, z + 1) + rd.getBrightness(x + 1, y, z + 1)
                    + rd.getBrightness(x + 1, y - 1, z + 1))
                 / 4.0f;
            col3 = (col3 + rd.getBrightness(x, y + 1, z + 1) + rd.getBrightness(x + 1, y, z + 1)
                    + rd.getBrightness(x + 1, y + 1, z + 1))
                 / 4.0f;
            col4 = (col4 + rd.getBrightness(x, y + 1, z + 1) + rd.getBrightness(x - 1, y, z + 1)
                    + rd.getBrightness(x - 1, y + 1, z + 1))
                 / 4.0f;
        }
        col1 /= MaxBrightness, col2 /= MaxBrightness, col3 /= MaxBrightness, col4 /= MaxBrightness;
        if (!AdvancedRender)
            col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f, col4 *= 0.5f;
        Renderer::Attrib1f(static_cast<float>(bl));
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
    if (!(bl == BlockID::AIR || bl == neighbors[5] && bl != BlockID::LEAF || BlockInfo(neighbors[5]).isOpaque())) {
        if (NiceGrass && bl == BlockID::GRASS && rd.getBlock(x, y - 1, z - 1) == BlockID::GRASS)
            tex = Textures::getTextureIndex(bl, 0);
        else
            tex = Textures::getTextureIndex(bl, 1);
        col1 = col2 = col3 = col4 = rd.getBrightness(x, y, z - 1);
        if (SmoothLighting) {
            col1 = (col1 + rd.getBrightness(x, y - 1, z - 1) + rd.getBrightness(x - 1, y, z - 1)
                    + rd.getBrightness(x - 1, y - 1, z - 1))
                 / 4.0f;
            col2 = (col2 + rd.getBrightness(x, y + 1, z - 1) + rd.getBrightness(x - 1, y, z - 1)
                    + rd.getBrightness(x - 1, y + 1, z - 1))
                 / 4.0f;
            col3 = (col3 + rd.getBrightness(x, y + 1, z - 1) + rd.getBrightness(x + 1, y, z - 1)
                    + rd.getBrightness(x + 1, y + 1, z - 1))
                 / 4.0f;
            col4 = (col4 + rd.getBrightness(x, y - 1, z - 1) + rd.getBrightness(x + 1, y, z - 1)
                    + rd.getBrightness(x + 1, y - 1, z - 1))
                 / 4.0f;
        }
        col1 /= MaxBrightness, col2 /= MaxBrightness, col3 /= MaxBrightness, col4 /= MaxBrightness;
        if (!AdvancedRender)
            col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f, col4 *= 0.5f;
        Renderer::Attrib1f(static_cast<float>(bl));
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
    float col0 = (float) p.col0 * 0.25f / MaxBrightness;
    float col1 = (float) p.col1 * 0.25f / MaxBrightness;
    float col2 = (float) p.col2 * 0.25f / MaxBrightness;
    float col3 = (float) p.col3 * 0.25f / MaxBrightness;
    int x = p.x, y = p.y, z = p.z, length = p.length;

    Renderer::Attrib1f(static_cast<float>(p.blk));
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

std::vector<Renderer::VertexBuffer> RenderChunk(std::array<Chunk const*, 27> neighbors) {
    auto res = std::vector<Renderer::VertexBuffer>();
    auto rd = ChunkRenderData(neighbors);
    for (size_t steps = 0; steps < 2; steps++) {
        if (AdvancedRender)
            Renderer::Begin(GL_QUADS, 3, 3, 1, 3, 1);
        else
            Renderer::Begin(GL_QUADS, 3, 3, 1);
        for (int x = 0; x < 16; x++)
            for (int y = 0; y < 16; y++)
                for (int z = 0; z < 16; z++) {
                    auto const& info = BlockInfo(rd.getBlock(x, y, z));
                    if (steps == 0 && !info.isTranslucent() || steps == 1 && info.isTranslucent()) {
                        RenderBlock(x, y, z, rd);
                    }
                }
        res.emplace_back(Renderer::End(true));
    }
    return res;
}

std::vector<Renderer::VertexBuffer> MergeFaceRenderChunk(std::array<Chunk const*, 27> neighbors) {
    auto res = std::vector<Renderer::VertexBuffer>();
    auto rd = ChunkRenderData(neighbors);
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
                        BlockID bl = rd.getBlock(x, y, z);
                        BlockID neighbour = rd.getBlock(x + dx, y + dy, z + dz);
                        auto const& info = BlockInfo(bl);
                        if (bl == BlockID::AIR || bl == neighbour && bl != BlockID::LEAF
                            || BlockInfo(neighbour).isOpaque()
                            || !(steps == 0 && !info.isTranslucent() || steps == 1 && info.isTranslucent())) {
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
                        TextureIndex tex = Textures::getTextureIndex(bl, face);
                        int br = 0, col0 = 0, col1 = 0, col2 = 0, col3 = 0;
                        switch (d) {
                            case 0:
                                if (NiceGrass && bl == BlockID::GRASS && rd.getBlock(x + 1, y - 1, z) == BlockID::GRASS)
                                    tex = Textures::getTextureIndex(bl, 0);
                                br = rd.getBrightness(x + 1, y, z);
                                if (SmoothLighting) {
                                    col0 = br + rd.getBrightness(x + 1, y - 1, z) + rd.getBrightness(x + 1, y, z - 1)
                                         + rd.getBrightness(x + 1, y - 1, z - 1);
                                    col1 = br + rd.getBrightness(x + 1, y + 1, z) + rd.getBrightness(x + 1, y, z - 1)
                                         + rd.getBrightness(x + 1, y + 1, z - 1);
                                    col2 = br + rd.getBrightness(x + 1, y + 1, z) + rd.getBrightness(x + 1, y, z + 1)
                                         + rd.getBrightness(x + 1, y + 1, z + 1);
                                    col3 = br + rd.getBrightness(x + 1, y - 1, z) + rd.getBrightness(x + 1, y, z + 1)
                                         + rd.getBrightness(x + 1, y - 1, z + 1);
                                } else
                                    col0 = col1 = col2 = col3 = br * 4;
                                break;
                            case 1:
                                if (NiceGrass && bl == BlockID::GRASS && rd.getBlock(x - 1, y - 1, z) == BlockID::GRASS)
                                    tex = Textures::getTextureIndex(bl, 0);
                                br = rd.getBrightness(x - 1, y, z);
                                if (SmoothLighting) {
                                    col0 = br + rd.getBrightness(x - 1, y + 1, z) + rd.getBrightness(x - 1, y, z - 1)
                                         + rd.getBrightness(x - 1, y + 1, z - 1);
                                    col1 = br + rd.getBrightness(x - 1, y - 1, z) + rd.getBrightness(x - 1, y, z - 1)
                                         + rd.getBrightness(x - 1, y - 1, z - 1);
                                    col2 = br + rd.getBrightness(x - 1, y - 1, z) + rd.getBrightness(x - 1, y, z + 1)
                                         + rd.getBrightness(x - 1, y - 1, z + 1);
                                    col3 = br + rd.getBrightness(x - 1, y + 1, z) + rd.getBrightness(x - 1, y, z + 1)
                                         + rd.getBrightness(x - 1, y + 1, z + 1);
                                } else
                                    col0 = col1 = col2 = col3 = br * 4;
                                break;
                            case 2:
                                br = rd.getBrightness(x, y + 1, z);
                                if (SmoothLighting) {
                                    col0 = br + rd.getBrightness(x + 1, y + 1, z) + rd.getBrightness(x, y + 1, z - 1)
                                         + rd.getBrightness(x + 1, y + 1, z - 1);
                                    col1 = br + rd.getBrightness(x - 1, y + 1, z) + rd.getBrightness(x, y + 1, z - 1)
                                         + rd.getBrightness(x - 1, y + 1, z - 1);
                                    col2 = br + rd.getBrightness(x - 1, y + 1, z) + rd.getBrightness(x, y + 1, z + 1)
                                         + rd.getBrightness(x - 1, y + 1, z + 1);
                                    col3 = br + rd.getBrightness(x + 1, y + 1, z) + rd.getBrightness(x, y + 1, z + 1)
                                         + rd.getBrightness(x + 1, y + 1, z + 1);
                                } else
                                    col0 = col1 = col2 = col3 = br * 4;
                                break;
                            case 3:
                                br = rd.getBrightness(x, y - 1, z);
                                if (SmoothLighting) {
                                    col0 = br + rd.getBrightness(x - 1, y - 1, z) + rd.getBrightness(x, y - 1, z - 1)
                                         + rd.getBrightness(x - 1, y - 1, z - 1);
                                    col1 = br + rd.getBrightness(x + 1, y - 1, z) + rd.getBrightness(x, y - 1, z - 1)
                                         + rd.getBrightness(x + 1, y - 1, z - 1);
                                    col2 = br + rd.getBrightness(x + 1, y - 1, z) + rd.getBrightness(x, y - 1, z + 1)
                                         + rd.getBrightness(x + 1, y - 1, z + 1);
                                    col3 = br + rd.getBrightness(x - 1, y - 1, z) + rd.getBrightness(x, y - 1, z + 1)
                                         + rd.getBrightness(x - 1, y - 1, z + 1);
                                } else
                                    col0 = col1 = col2 = col3 = br * 4;
                                break;
                            case 4:
                                if (NiceGrass && bl == BlockID::GRASS && rd.getBlock(x, y - 1, z + 1) == BlockID::GRASS)
                                    tex = Textures::getTextureIndex(bl, 0);
                                br = rd.getBrightness(x, y, z + 1);
                                if (SmoothLighting) {
                                    col0 = br + rd.getBrightness(x - 1, y, z + 1) + rd.getBrightness(x, y + 1, z + 1)
                                         + rd.getBrightness(x - 1, y + 1, z + 1);
                                    col1 = br + rd.getBrightness(x - 1, y, z + 1) + rd.getBrightness(x, y - 1, z + 1)
                                         + rd.getBrightness(x - 1, y - 1, z + 1);
                                    col2 = br + rd.getBrightness(x + 1, y, z + 1) + rd.getBrightness(x, y - 1, z + 1)
                                         + rd.getBrightness(x + 1, y - 1, z + 1);
                                    col3 = br + rd.getBrightness(x + 1, y, z + 1) + rd.getBrightness(x, y + 1, z + 1)
                                         + rd.getBrightness(x + 1, y + 1, z + 1);
                                } else
                                    col0 = col1 = col2 = col3 = br * 4;
                                break;
                            case 5:
                                if (NiceGrass && bl == BlockID::GRASS && rd.getBlock(x, y - 1, z - 1) == BlockID::GRASS)
                                    tex = Textures::getTextureIndex(bl, 0);
                                br = rd.getBrightness(x, y, z - 1);
                                if (SmoothLighting) {
                                    col0 = br + rd.getBrightness(x - 1, y, z - 1) + rd.getBrightness(x, y - 1, z - 1)
                                         + rd.getBrightness(x - 1, y - 1, z - 1);
                                    col1 = br + rd.getBrightness(x - 1, y, z - 1) + rd.getBrightness(x, y + 1, z - 1)
                                         + rd.getBrightness(x - 1, y + 1, z - 1);
                                    col2 = br + rd.getBrightness(x + 1, y, z - 1) + rd.getBrightness(x, y + 1, z - 1)
                                         + rd.getBrightness(x + 1, y + 1, z - 1);
                                    col3 = br + rd.getBrightness(x + 1, y, z - 1) + rd.getBrightness(x, y - 1, z - 1)
                                         + rd.getBrightness(x + 1, y - 1, z - 1);
                                } else
                                    col0 = col1 = col2 = col3 = br * 4;
                                break;
                        }
                        // Render
                        bool once = col0 != col1 || col1 != col2 || col2 != col3;
                        if (valid) {
                            if (once || cur.once || bl != cur.blk || tex != cur.tex || col0 != cur.col0) {
                                RenderPrimitive(cur);
                                cur.x = x;
                                cur.y = y;
                                cur.z = z;
                                cur.length = 0;
                                cur.direction = d;
                                cur.once = once;
                                cur.blk = bl;
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
                            cur.blk = bl;
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

void Chunk::buildMeshes(std::array<Chunk const*, 27> const& neighbors) {
    // Build new VBOs
    meshes = MergeFace ? MergeFaceRenderChunk(neighbors) : RenderChunk(neighbors);

    // Update flags
    if (!isMeshed)
        loadAnim = ccoord.y * 16.0f + 16.0f;
    isMeshed = true;
    isUpdated = false;

    meshedChunks++;
    updatedChunks++;
}

module;

#include <glad/gl.h>
#include "../../cmake-build-release/_deps/klsxx-src/Essential/Published/kls/temp/Temp.h"
#undef assert

module chunks;
import std;
import types;
import math;
import debug;
import blocks;
import rendering;
import textures;
import globals;

namespace chunks {

// One face in merge face
struct QuadPrimitive {
    int x = 0, y = 0, z = 0, length = 0, direction = 0;
    // If the vertexes have different colors (smoothed lighting), the primitive cannot connect with others.
    // This variable indicates whether the vertexes have different colors.
    bool once = false;
    // Vertex colors.
    int col0 = 0, col1 = 0, col2 = 0, col3 = 0;
    // Block ID.
    blocks::Id blk = blocks::Id(0);
    // Texture index.
    TextureIndex tex = TextureIndex::NULLBLOCK;
};

// Before rendering, the whole chunk and neighboring blocks are converted into this structure.
//
// * Since merge face rendering goes through the whole chunk 12 times, it is beneficial to
//   avoid repeatedly looking up block properties in the block info registry.
// * All block properties relevant to rendering should be stored in this structure.
class BlockRenderData {
public:
    BlockRenderData() = default;
    BlockRenderData(blocks::BlockData block):
        _id(block.id) {
        auto const& info = block_info(_id);

        // Temporary: mix two light levels.
        _light = std::max(block.light.sky(), block.light.block());

        // Temporary: clamp to minimum light level 2.
        if (!info.opaque)
            _light = std::max(_light, uint8_t{2});

        if (_id == base_blocks().air)
            _flags |= 0x1;
        if (info.opaque)
            _flags |= 0x2;
        if (info.translucent)
            _flags |= 0x4;
    }

    auto id() const -> blocks::Id {
        return _id;
    }
    auto light() const -> std::uint8_t {
        return _light;
    }
    auto skipped() const -> bool {
        return _flags & 0x1;
    }
    auto opaque() const -> bool {
        return _flags & 0x2;
    }
    auto translucent() const -> bool {
        return _flags & 0x4;
    }

    auto should_render(size_t steps) -> bool {
        if (skipped())
            return false;
        if (steps == 0 && translucent())
            return false;
        if (steps == 1 && !translucent())
            return false;
        return true;
    }

    auto should_render_face(BlockRenderData neighbor, size_t steps) -> bool {
        if (neighbor.opaque())
            return false;
        if (_id == neighbor._id && _id != base_blocks().leaf)
            return false;
        return true;
    }

private:
    blocks::Id _id = {};
    std::uint8_t _light = 0;
    std::uint8_t _flags = 0;
};

// Temporary: maximum value obtained after mixing two light levels.
constexpr auto MAX_LIGHT = 15.0f;

// All data needed to render a chunk.
class ChunkRenderData {
public:
    ChunkRenderData(Vec3i ccoord, std::array<Chunk const*, 3 * 3 * 3> neighbors) {
        auto index = 0uz;
        for (int x = -1; x < Chunk::SIZE + 1; x++) {
            auto rcx = x >> Chunk::SIZE_LOG;
            auto bx = static_cast<uint32_t>(x) & (Chunk::SIZE - 1);
            for (int y = -1; y < Chunk::SIZE + 1; y++) {
                auto rcy = y >> Chunk::SIZE_LOG;
                auto by = static_cast<uint32_t>(y) & (Chunk::SIZE - 1);
                for (int z = -1; z < Chunk::SIZE + 1; z++) {
                    auto rcz = z >> Chunk::SIZE_LOG;
                    auto bz = static_cast<uint32_t>(z) & (Chunk::SIZE - 1);
                    auto& block = _data[index++];
                    auto cptr = neighbors[((rcx + 1) * 3 + rcy + 1) * 3 + rcz + 1];
                    assert(cptr, "neighbors array should not contain null pointer");
                    if (cptr == chunks::EMPTY_CHUNK) {
                        auto light = (ccoord.y() + rcy < 0) ? blocks::NO_LIGHT : blocks::SKY_LIGHT;
                        block = blocks::BlockData{.id = base_blocks().air, .light = light};
                    } else {
                        block = cptr->block(Vec3u(bx, by, bz));
                    }
                }
            }
        }
    }

    auto block(int x, int y, int z) const -> BlockRenderData {
        return _data[((x + 1) * (Chunk::SIZE + 2) + y + 1) * (Chunk::SIZE + 2) + z + 1];
    }

private:
    std::array<BlockRenderData, (Chunk::SIZE + 2) * (Chunk::SIZE + 2) * (Chunk::SIZE + 2)> _data = {};
};

// The default method for rendering a block.
void _render_block(int x, int y, int z, size_t steps, ChunkRenderData const& rd, Renderer::VertexBuilder& vb) {
    auto bl = rd.block(x, y, z);
    if (!bl.should_render(steps))
        return;

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
    if (bl.should_render_face(neighbors[0], steps)) {
        if (NiceGrass && bl.id() == base_blocks().grass && rd.block(x + 1, y - 1, z).id() == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id(), 0);
        else
            tex = Textures::getTextureIndex(bl.id(), 1);
        col1 = col2 = col3 = col4 = rd.block(x + 1, y, z).light();
        if (SmoothLighting) {
            col1 = (col1 + rd.block(x + 1, y - 1, z).light() + rd.block(x + 1, y, z - 1).light()
                    + rd.block(x + 1, y - 1, z - 1).light())
                 / 4.0f;
            col2 = (col2 + rd.block(x + 1, y + 1, z).light() + rd.block(x + 1, y, z - 1).light()
                    + rd.block(x + 1, y + 1, z - 1).light())
                 / 4.0f;
            col3 = (col3 + rd.block(x + 1, y + 1, z).light() + rd.block(x + 1, y, z + 1).light()
                    + rd.block(x + 1, y + 1, z + 1).light())
                 / 4.0f;
            col4 = (col4 + rd.block(x + 1, y - 1, z).light() + rd.block(x + 1, y, z + 1).light()
                    + rd.block(x + 1, y - 1, z + 1).light())
                 / 4.0f;
        }
        col1 /= MAX_LIGHT, col2 /= MAX_LIGHT, col3 /= MAX_LIGHT, col4 /= MAX_LIGHT;
        if (!AdvancedRender)
            col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f, col4 *= 0.7f;
        vb.Attrib1f(static_cast<float>(bl.id().get()));
        vb.TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
        vb.Normal3f(1.0f, 0.0f, 0.0f);
        vb.Color3f(col1, col1, col1);
        vb.TexCoord2f(1.0f, 0.0f);
        vb.Vertex3f(0.5f + x, -0.5f + y, -0.5f + z);
        vb.Color3f(col2, col2, col2);
        vb.TexCoord2f(1.0f, 1.0f);
        vb.Vertex3f(0.5f + x, 0.5f + y, -0.5f + z);
        vb.Color3f(col3, col3, col3);
        vb.TexCoord2f(0.0f, 1.0f);
        vb.Vertex3f(0.5f + x, 0.5f + y, 0.5f + z);
        vb.Color3f(col4, col4, col4);
        vb.TexCoord2f(0.0f, 0.0f);
        vb.Vertex3f(0.5f + x, -0.5f + y, 0.5f + z);
    }

    // Left Face
    if (bl.should_render_face(neighbors[1], steps)) {
        if (NiceGrass && bl.id() == base_blocks().grass && rd.block(x - 1, y - 1, z).id() == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id(), 0);
        else
            tex = Textures::getTextureIndex(bl.id(), 1);
        col1 = col2 = col3 = col4 = rd.block(x - 1, y, z).light();
        if (SmoothLighting) {
            col1 = (col1 + rd.block(x - 1, y - 1, z).light() + rd.block(x - 1, y, z - 1).light()
                    + rd.block(x - 1, y - 1, z - 1).light())
                 / 4.0f;
            col2 = (col2 + rd.block(x - 1, y - 1, z).light() + rd.block(x - 1, y, z + 1).light()
                    + rd.block(x - 1, y - 1, z + 1).light())
                 / 4.0f;
            col3 = (col3 + rd.block(x - 1, y + 1, z).light() + rd.block(x - 1, y, z + 1).light()
                    + rd.block(x - 1, y + 1, z + 1).light())
                 / 4.0f;
            col4 = (col4 + rd.block(x - 1, y + 1, z).light() + rd.block(x - 1, y, z - 1).light()
                    + rd.block(x - 1, y + 1, z - 1).light())
                 / 4.0f;
        }
        col1 /= MAX_LIGHT, col2 /= MAX_LIGHT, col3 /= MAX_LIGHT, col4 /= MAX_LIGHT;
        if (!AdvancedRender)
            col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f, col4 *= 0.7f;
        vb.Attrib1f(static_cast<float>(bl.id().get()));
        vb.TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
        vb.Normal3f(-1.0f, 0.0f, 0.0f);
        vb.Color3f(col1, col1, col1);
        vb.TexCoord2f(0.0f, 0.0f);
        vb.Vertex3f(-0.5f + x, -0.5f + y, -0.5f + z);
        vb.Color3f(col2, col2, col2);
        vb.TexCoord2f(1.0f, 0.0f);
        vb.Vertex3f(-0.5f + x, -0.5f + y, 0.5f + z);
        vb.Color3f(col3, col3, col3);
        vb.TexCoord2f(1.0f, 1.0f);
        vb.Vertex3f(-0.5f + x, 0.5f + y, 0.5f + z);
        vb.Color3f(col4, col4, col4);
        vb.TexCoord2f(0.0f, 1.0f);
        vb.Vertex3f(-0.5f + x, 0.5f + y, -0.5f + z);
    }

    // Top Face
    if (bl.should_render_face(neighbors[2], steps)) {
        tex = Textures::getTextureIndex(bl.id(), 0);
        col1 = col2 = col3 = col4 = rd.block(x, y + 1, z).light();
        if (SmoothLighting) {
            col1 = (col1 + rd.block(x, y + 1, z - 1).light() + rd.block(x - 1, y + 1, z).light()
                    + rd.block(x - 1, y + 1, z - 1).light())
                 / 4.0f;
            col2 = (col2 + rd.block(x, y + 1, z + 1).light() + rd.block(x - 1, y + 1, z).light()
                    + rd.block(x - 1, y + 1, z + 1).light())
                 / 4.0f;
            col3 = (col3 + rd.block(x, y + 1, z + 1).light() + rd.block(x + 1, y + 1, z).light()
                    + rd.block(x + 1, y + 1, z + 1).light())
                 / 4.0f;
            col4 = (col4 + rd.block(x, y + 1, z - 1).light() + rd.block(x + 1, y + 1, z).light()
                    + rd.block(x + 1, y + 1, z - 1).light())
                 / 4.0f;
        }
        col1 /= MAX_LIGHT, col2 /= MAX_LIGHT, col3 /= MAX_LIGHT, col4 /= MAX_LIGHT;
        vb.Attrib1f(static_cast<float>(bl.id().get()));
        vb.TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
        vb.Normal3f(0.0f, 1.0f, 0.0f);
        vb.Color3f(col1, col1, col1);
        vb.TexCoord2f(0.0f, 1.0f);
        vb.Vertex3f(-0.5f + x, 0.5f + y, -0.5f + z);
        vb.Color3f(col2, col2, col2);
        vb.TexCoord2f(0.0f, 0.0f);
        vb.Vertex3f(-0.5f + x, 0.5f + y, 0.5f + z);
        vb.Color3f(col3, col3, col3);
        vb.TexCoord2f(1.0f, 0.0f);
        vb.Vertex3f(0.5f + x, 0.5f + y, 0.5f + z);
        vb.Color3f(col4, col4, col4);
        vb.TexCoord2f(1.0f, 1.0f);
        vb.Vertex3f(0.5f + x, 0.5f + y, -0.5f + z);
    }

    // Bottom Face
    if (bl.should_render_face(neighbors[3], steps)) {
        tex = Textures::getTextureIndex(bl.id(), 2);
        col1 = col2 = col3 = col4 = rd.block(x, y - 1, z).light();
        if (SmoothLighting) {
            col1 = (col1 + rd.block(x, y - 1, z - 1).light() + rd.block(x - 1, y - 1, z).light()
                    + rd.block(x - 1, y - 1, z - 1).light())
                 / 4.0f;
            col2 = (col2 + rd.block(x, y - 1, z - 1).light() + rd.block(x + 1, y - 1, z).light()
                    + rd.block(x + 1, y - 1, z - 1).light())
                 / 4.0f;
            col3 = (col3 + rd.block(x, y - 1, z + 1).light() + rd.block(x + 1, y - 1, z).light()
                    + rd.block(x + 1, y - 1, z + 1).light())
                 / 4.0f;
            col4 = (col4 + rd.block(x, y - 1, z + 1).light() + rd.block(x - 1, y - 1, z).light()
                    + rd.block(x - 1, y - 1, z + 1).light())
                 / 4.0f;
        }
        col1 /= MAX_LIGHT, col2 /= MAX_LIGHT, col3 /= MAX_LIGHT, col4 /= MAX_LIGHT;
        vb.Attrib1f(static_cast<float>(bl.id().get()));
        vb.TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
        vb.Normal3f(0.0f, -1.0f, 0.0f);
        vb.Color3f(col1, col1, col1);
        vb.TexCoord2f(1.0f, 1.0f);
        vb.Vertex3f(-0.5f + x, -0.5f + y, -0.5f + z);
        vb.Color3f(col2, col2, col2);
        vb.TexCoord2f(0.0f, 1.0f);
        vb.Vertex3f(0.5f + x, -0.5f + y, -0.5f + z);
        vb.Color3f(col3, col3, col3);
        vb.TexCoord2f(0.0f, 0.0f);
        vb.Vertex3f(0.5f + x, -0.5f + y, 0.5f + z);
        vb.Color3f(col4, col4, col4);
        vb.TexCoord2f(1.0f, 0.0f);
        vb.Vertex3f(-0.5f + x, -0.5f + y, 0.5f + z);
    }

    // Front Face
    if (bl.should_render_face(neighbors[4], steps)) {
        if (NiceGrass && bl.id() == base_blocks().grass && rd.block(x, y - 1, z + 1).id() == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id(), 0);
        else
            tex = Textures::getTextureIndex(bl.id(), 1);
        col1 = col2 = col3 = col4 = rd.block(x, y, z + 1).light();
        if (SmoothLighting) {
            col1 = (col1 + rd.block(x, y - 1, z + 1).light() + rd.block(x - 1, y, z + 1).light()
                    + rd.block(x - 1, y - 1, z + 1).light())
                 / 4.0f;
            col2 = (col2 + rd.block(x, y - 1, z + 1).light() + rd.block(x + 1, y, z + 1).light()
                    + rd.block(x + 1, y - 1, z + 1).light())
                 / 4.0f;
            col3 = (col3 + rd.block(x, y + 1, z + 1).light() + rd.block(x + 1, y, z + 1).light()
                    + rd.block(x + 1, y + 1, z + 1).light())
                 / 4.0f;
            col4 = (col4 + rd.block(x, y + 1, z + 1).light() + rd.block(x - 1, y, z + 1).light()
                    + rd.block(x - 1, y + 1, z + 1).light())
                 / 4.0f;
        }
        col1 /= MAX_LIGHT, col2 /= MAX_LIGHT, col3 /= MAX_LIGHT, col4 /= MAX_LIGHT;
        if (!AdvancedRender)
            col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f, col4 *= 0.5f;
        vb.Attrib1f(static_cast<float>(bl.id().get()));
        vb.TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
        vb.Normal3f(0.0f, 0.0f, 1.0f);
        vb.Color3f(col1, col1, col1);
        vb.TexCoord2f(0.0f, 0.0f);
        vb.Vertex3f(-0.5f + x, -0.5f + y, 0.5f + z);
        vb.Color3f(col2, col2, col2);
        vb.TexCoord2f(1.0f, 0.0f);
        vb.Vertex3f(0.5f + x, -0.5f + y, 0.5f + z);
        vb.Color3f(col3, col3, col3);
        vb.TexCoord2f(1.0f, 1.0f);
        vb.Vertex3f(0.5f + x, 0.5f + y, 0.5f + z);
        vb.Color3f(col4, col4, col4);
        vb.TexCoord2f(0.0f, 1.0f);
        vb.Vertex3f(-0.5f + x, 0.5f + y, 0.5f + z);
    }

    // Back Face
    if (bl.should_render_face(neighbors[5], steps)) {
        if (NiceGrass && bl.id() == base_blocks().grass && rd.block(x, y - 1, z - 1).id() == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id(), 0);
        else
            tex = Textures::getTextureIndex(bl.id(), 1);
        col1 = col2 = col3 = col4 = rd.block(x, y, z - 1).light();
        if (SmoothLighting) {
            col1 = (col1 + rd.block(x, y - 1, z - 1).light() + rd.block(x - 1, y, z - 1).light()
                    + rd.block(x - 1, y - 1, z - 1).light())
                 / 4.0f;
            col2 = (col2 + rd.block(x, y + 1, z - 1).light() + rd.block(x - 1, y, z - 1).light()
                    + rd.block(x - 1, y + 1, z - 1).light())
                 / 4.0f;
            col3 = (col3 + rd.block(x, y + 1, z - 1).light() + rd.block(x + 1, y, z - 1).light()
                    + rd.block(x + 1, y + 1, z - 1).light())
                 / 4.0f;
            col4 = (col4 + rd.block(x, y - 1, z - 1).light() + rd.block(x + 1, y, z - 1).light()
                    + rd.block(x + 1, y - 1, z - 1).light())
                 / 4.0f;
        }
        col1 /= MAX_LIGHT, col2 /= MAX_LIGHT, col3 /= MAX_LIGHT, col4 /= MAX_LIGHT;
        if (!AdvancedRender)
            col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f, col4 *= 0.5f;
        vb.Attrib1f(static_cast<float>(bl.id().get()));
        vb.TexCoord3f(0.0f, 0.0f, static_cast<float>(tex));
        vb.Normal3f(0.0f, 0.0f, -1.0f);
        vb.Color3f(col1, col1, col1);
        vb.TexCoord2f(1.0f, 0.0f);
        vb.Vertex3f(-0.5f + x, -0.5f + y, -0.5f + z);
        vb.Color3f(col2, col2, col2);
        vb.TexCoord2f(1.0f, 1.0f);
        vb.Vertex3f(-0.5f + x, 0.5f + y, -0.5f + z);
        vb.Color3f(col3, col3, col3);
        vb.TexCoord2f(0.0f, 1.0f);
        vb.Vertex3f(0.5f + x, 0.5f + y, -0.5f + z);
        vb.Color3f(col4, col4, col4);
        vb.TexCoord2f(0.0f, 0.0f);
        vb.Vertex3f(0.5f + x, -0.5f + y, -0.5f + z);
    }
}

// The merge face rendering method for a primitive (adjacent block faces).
void _render_primitive(QuadPrimitive const& p, Renderer::VertexBuilder& vb) {
    float col0 = (float) p.col0 * 0.25f / MAX_LIGHT;
    float col1 = (float) p.col1 * 0.25f / MAX_LIGHT;
    float col2 = (float) p.col2 * 0.25f / MAX_LIGHT;
    float col3 = (float) p.col3 * 0.25f / MAX_LIGHT;
    int x = p.x, y = p.y, z = p.z, length = p.length;

    vb.Attrib1f(static_cast<float>(p.blk.get()));
    vb.TexCoord3f(0.0f, 0.0f, static_cast<float>(p.tex));

    switch (p.direction) {
        case 0:
            if (!AdvancedRender)
                col0 *= 0.7f, col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f;
            vb.Normal3f(1.0f, 0.0f, 0.0f);
            vb.Color3f(col0, col0, col0);
            vb.TexCoord2f(0.0f, 0.0f);
            vb.Vertex3f(x + 0.5f, y - 0.5f, z - 0.5f);
            vb.Color3f(col1, col1, col1);
            vb.TexCoord2f(0.0f, 1.0f);
            vb.Vertex3f(x + 0.5f, y + 0.5f, z - 0.5f);
            vb.Color3f(col2, col2, col2);
            vb.TexCoord2f(length + 1.0f, 1.0f);
            vb.Vertex3f(x + 0.5f, y + 0.5f, z + length + 0.5f);
            vb.Color3f(col3, col3, col3);
            vb.TexCoord2f(length + 1.0f, 0.0f);
            vb.Vertex3f(x + 0.5f, y - 0.5f, z + length + 0.5f);
            break;
        case 1:
            if (!AdvancedRender)
                col0 *= 0.7f, col1 *= 0.7f, col2 *= 0.7f, col3 *= 0.7f;
            vb.Normal3f(-1.0f, 0.0f, 0.0f);
            vb.Color3f(col0, col0, col0);
            vb.TexCoord2f(0.0f, 1.0f);
            vb.Vertex3f(x - 0.5f, y + 0.5f, z - 0.5f);
            vb.Color3f(col1, col1, col1);
            vb.TexCoord2f(0.0f, 0.0f);
            vb.Vertex3f(x - 0.5f, y - 0.5f, z - 0.5f);
            vb.Color3f(col2, col2, col2);
            vb.TexCoord2f(length + 1.0f, 0.0f);
            vb.Vertex3f(x - 0.5f, y - 0.5f, z + length + 0.5f);
            vb.Color3f(col3, col3, col3);
            vb.TexCoord2f(length + 1.0f, 1.0f);
            vb.Vertex3f(x - 0.5f, y + 0.5f, z + length + 0.5f);
            break;
        case 2:
            vb.Normal3f(0.0f, 1.0f, 0.0f);
            vb.Color3f(col0, col0, col0);
            vb.TexCoord2f(0.0f, 0.0f);
            vb.Vertex3f(x + 0.5f, y + 0.5f, z - 0.5f);
            vb.Color3f(col1, col1, col1);
            vb.TexCoord2f(0.0f, 1.0f);
            vb.Vertex3f(x - 0.5f, y + 0.5f, z - 0.5f);
            vb.Color3f(col2, col2, col2);
            vb.TexCoord2f(length + 1.0f, 1.0f);
            vb.Vertex3f(x - 0.5f, y + 0.5f, z + length + 0.5f);
            vb.Color3f(col3, col3, col3);
            vb.TexCoord2f(length + 1.0f, 0.0f);
            vb.Vertex3f(x + 0.5f, y + 0.5f, z + length + 0.5f);
            break;
        case 3:
            vb.Normal3f(0.0f, -1.0f, 0.0f);
            vb.Color3f(col0, col0, col0);
            vb.TexCoord2f(0.0f, 0.0f);
            vb.Vertex3f(x - 0.5f, y - 0.5f, z - 0.5f);
            vb.Color3f(col1, col1, col1);
            vb.TexCoord2f(0.0f, 1.0f);
            vb.Vertex3f(x + 0.5f, y - 0.5f, z - 0.5f);
            vb.Color3f(col2, col2, col2);
            vb.TexCoord2f(length + 1.0f, 1.0f);
            vb.Vertex3f(x + 0.5f, y - 0.5f, z + length + 0.5f);
            vb.Color3f(col3, col3, col3);
            vb.TexCoord2f(length + 1.0f, 0.0f);
            vb.Vertex3f(x - 0.5f, y - 0.5f, z + length + 0.5f);
            break;
        case 4:
            if (!AdvancedRender)
                col0 *= 0.5f, col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f;
            vb.Normal3f(0.0f, 0.0f, 1.0f);
            vb.Color3f(col0, col0, col0);
            vb.TexCoord2f(0.0f, length + 1.0f);
            vb.Vertex3f(x - 0.5f, y + length + 0.5f, z + 0.5f);
            vb.Color3f(col1, col1, col1);
            vb.TexCoord2f(0.0f, 0.0f);
            vb.Vertex3f(x - 0.5f, y - 0.5f, z + 0.5f);
            vb.Color3f(col2, col2, col2);
            vb.TexCoord2f(1.0f, 0.0f);
            vb.Vertex3f(x + 0.5f, y - 0.5f, z + 0.5f);
            vb.Color3f(col3, col3, col3);
            vb.TexCoord2f(1.0f, length + 1.0f);
            vb.Vertex3f(x + 0.5f, y + length + 0.5f, z + 0.5f);
            break;
        case 5:
            if (!AdvancedRender)
                col0 *= 0.5f, col1 *= 0.5f, col2 *= 0.5f, col3 *= 0.5f;
            vb.Normal3f(0.0f, 0.0f, -1.0f);
            vb.Color3f(col0, col0, col0);
            vb.TexCoord2f(0.0f, 0.0f);
            vb.Vertex3f(x - 0.5f, y - 0.5f, z - 0.5f);
            vb.Color3f(col1, col1, col1);
            vb.TexCoord2f(0.0f, length + 1.0f);
            vb.Vertex3f(x - 0.5f, y + length + 0.5f, z - 0.5f);
            vb.Color3f(col2, col2, col2);
            vb.TexCoord2f(1.0f, length + 1.0f);
            vb.Vertex3f(x + 0.5f, y + length + 0.5f, z - 0.5f);
            vb.Color3f(col3, col3, col3);
            vb.TexCoord2f(1.0f, 0.0f);
            vb.Vertex3f(x + 0.5f, y - 0.5f, z - 0.5f);
            break;
    }
}

// The default method for rendering a chunk.
auto _render_chunk_pass(ChunkRenderData& rd, Renderer::VertexBuilder& vb, int pass) {
    for (int x = 0; x < Chunk::SIZE; x++)
        for (int y = 0; y < Chunk::SIZE; y++)
            for (int z = 0; z < Chunk::SIZE; z++) {
                _render_block(x, y, z, pass, rd, vb);
            }
}

// The merge face rendering method for a chunk.
//
// * For faces orienting X+ (d = 0) or X- (d = 1), the merge direction is Z+;
// * For faces orienting Y+ (d = 2) or Y- (d = 3), the merge direction is Z+;
// * For faces orienting Z+ (d = 4) or Z- (d = 5), the merge direction is Y+.
//
// The merge directions are determined by the fact that block render data are stored in a
// X-Y-Z-major order, which means blocks adjacent in the Z direction are stored consecutively.
// This probably allows for better cache hit rates.
auto _merge_face_render_chunk_pass(ChunkRenderData& rd, Renderer::VertexBuilder& vb, int pass) {
    for (int d = 0; d < 6; d++) {
        // Render current face
        for (int i = 0; i < Chunk::SIZE; i++)
            for (int j = 0; j < Chunk::SIZE; j++) {
                QuadPrimitive cur;
                bool valid = false;
                // Linear merge
                for (int k = 0; k < Chunk::SIZE; k++) {
                    // Get position (the coordinate assigned to `k` is the merge direction)
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
                            x = j, y = k, z = i, dz = 1;
                            break;
                        case 5:
                            x = j, y = k, z = i, dz = -1;
                            break;
                    }
                    // Get block render data
                    auto bl = rd.block(x, y, z);
                    // Check if block face should render. This appears to be bottlenecking
                    // since there are usually a lot of blocks that are not rendered.
                    if (!bl.should_render(pass) || !bl.should_render_face(rd.block(x + dx, y + dy, z + dz), pass)) {
                        if (valid) {
                            _render_primitive(cur, vb);
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
                    TextureIndex tex = Textures::getTextureIndex(bl.id(), face);
                    int br = 0, col0 = 0, col1 = 0, col2 = 0, col3 = 0;
                    switch (d) {
                        case 0:
                            if (NiceGrass && bl.id() == base_blocks().grass
                                && rd.block(x + 1, y - 1, z).id() == base_blocks().grass)
                                tex = Textures::getTextureIndex(bl.id(), 0);
                            br = rd.block(x + 1, y, z).light();
                            if (SmoothLighting) {
                                col0 = br + rd.block(x + 1, y - 1, z).light() + rd.block(x + 1, y, z - 1).light()
                                     + rd.block(x + 1, y - 1, z - 1).light();
                                col1 = br + rd.block(x + 1, y + 1, z).light() + rd.block(x + 1, y, z - 1).light()
                                     + rd.block(x + 1, y + 1, z - 1).light();
                                col2 = br + rd.block(x + 1, y + 1, z).light() + rd.block(x + 1, y, z + 1).light()
                                     + rd.block(x + 1, y + 1, z + 1).light();
                                col3 = br + rd.block(x + 1, y - 1, z).light() + rd.block(x + 1, y, z + 1).light()
                                     + rd.block(x + 1, y - 1, z + 1).light();
                            } else
                                col0 = col1 = col2 = col3 = br * 4;
                            break;
                        case 1:
                            if (NiceGrass && bl.id() == base_blocks().grass
                                && rd.block(x - 1, y - 1, z).id() == base_blocks().grass)
                                tex = Textures::getTextureIndex(bl.id(), 0);
                            br = rd.block(x - 1, y, z).light();
                            if (SmoothLighting) {
                                col0 = br + rd.block(x - 1, y + 1, z).light() + rd.block(x - 1, y, z - 1).light()
                                     + rd.block(x - 1, y + 1, z - 1).light();
                                col1 = br + rd.block(x - 1, y - 1, z).light() + rd.block(x - 1, y, z - 1).light()
                                     + rd.block(x - 1, y - 1, z - 1).light();
                                col2 = br + rd.block(x - 1, y - 1, z).light() + rd.block(x - 1, y, z + 1).light()
                                     + rd.block(x - 1, y - 1, z + 1).light();
                                col3 = br + rd.block(x - 1, y + 1, z).light() + rd.block(x - 1, y, z + 1).light()
                                     + rd.block(x - 1, y + 1, z + 1).light();
                            } else
                                col0 = col1 = col2 = col3 = br * 4;
                            break;
                        case 2:
                            br = rd.block(x, y + 1, z).light();
                            if (SmoothLighting) {
                                col0 = br + rd.block(x + 1, y + 1, z).light() + rd.block(x, y + 1, z - 1).light()
                                     + rd.block(x + 1, y + 1, z - 1).light();
                                col1 = br + rd.block(x - 1, y + 1, z).light() + rd.block(x, y + 1, z - 1).light()
                                     + rd.block(x - 1, y + 1, z - 1).light();
                                col2 = br + rd.block(x - 1, y + 1, z).light() + rd.block(x, y + 1, z + 1).light()
                                     + rd.block(x - 1, y + 1, z + 1).light();
                                col3 = br + rd.block(x + 1, y + 1, z).light() + rd.block(x, y + 1, z + 1).light()
                                     + rd.block(x + 1, y + 1, z + 1).light();
                            } else
                                col0 = col1 = col2 = col3 = br * 4;
                            break;
                        case 3:
                            br = rd.block(x, y - 1, z).light();
                            if (SmoothLighting) {
                                col0 = br + rd.block(x - 1, y - 1, z).light() + rd.block(x, y - 1, z - 1).light()
                                     + rd.block(x - 1, y - 1, z - 1).light();
                                col1 = br + rd.block(x + 1, y - 1, z).light() + rd.block(x, y - 1, z - 1).light()
                                     + rd.block(x + 1, y - 1, z - 1).light();
                                col2 = br + rd.block(x + 1, y - 1, z).light() + rd.block(x, y - 1, z + 1).light()
                                     + rd.block(x + 1, y - 1, z + 1).light();
                                col3 = br + rd.block(x - 1, y - 1, z).light() + rd.block(x, y - 1, z + 1).light()
                                     + rd.block(x - 1, y - 1, z + 1).light();
                            } else
                                col0 = col1 = col2 = col3 = br * 4;
                            break;
                        case 4:
                            if (NiceGrass && bl.id() == base_blocks().grass
                                && rd.block(x, y - 1, z + 1).id() == base_blocks().grass)
                                tex = Textures::getTextureIndex(bl.id(), 0);
                            br = rd.block(x, y, z + 1).light();
                            if (SmoothLighting) {
                                col0 = br + rd.block(x - 1, y, z + 1).light() + rd.block(x, y + 1, z + 1).light()
                                     + rd.block(x - 1, y + 1, z + 1).light();
                                col1 = br + rd.block(x - 1, y, z + 1).light() + rd.block(x, y - 1, z + 1).light()
                                     + rd.block(x - 1, y - 1, z + 1).light();
                                col2 = br + rd.block(x + 1, y, z + 1).light() + rd.block(x, y - 1, z + 1).light()
                                     + rd.block(x + 1, y - 1, z + 1).light();
                                col3 = br + rd.block(x + 1, y, z + 1).light() + rd.block(x, y + 1, z + 1).light()
                                     + rd.block(x + 1, y + 1, z + 1).light();
                            } else
                                col0 = col1 = col2 = col3 = br * 4;
                            break;
                        case 5:
                            if (NiceGrass && bl.id() == base_blocks().grass
                                && rd.block(x, y - 1, z - 1).id() == base_blocks().grass)
                                tex = Textures::getTextureIndex(bl.id(), 0);
                            br = rd.block(x, y, z - 1).light();
                            if (SmoothLighting) {
                                col0 = br + rd.block(x - 1, y, z - 1).light() + rd.block(x, y - 1, z - 1).light()
                                     + rd.block(x - 1, y - 1, z - 1).light();
                                col1 = br + rd.block(x - 1, y, z - 1).light() + rd.block(x, y + 1, z - 1).light()
                                     + rd.block(x - 1, y + 1, z - 1).light();
                                col2 = br + rd.block(x + 1, y, z - 1).light() + rd.block(x, y + 1, z - 1).light()
                                     + rd.block(x + 1, y + 1, z - 1).light();
                                col3 = br + rd.block(x + 1, y, z - 1).light() + rd.block(x, y - 1, z - 1).light()
                                     + rd.block(x + 1, y - 1, z - 1).light();
                            } else
                                col0 = col1 = col2 = col3 = br * 4;
                            break;
                    }
                    // Render
                    bool once = col0 != col1 || col1 != col2 || col2 != col3;
                    if (valid) {
                        if (once || cur.once || bl.id() != cur.blk || tex != cur.tex || col0 != cur.col0) {
                            _render_primitive(cur, vb);
                            cur.x = x;
                            cur.y = y;
                            cur.z = z;
                            cur.length = 0;
                            cur.direction = d;
                            cur.once = once;
                            cur.blk = bl.id();
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
                        cur.blk = bl.id();
                        cur.tex = tex;
                        cur.col0 = col0;
                        cur.col1 = col1;
                        cur.col2 = col2;
                        cur.col3 = col3;
                    }
                }
                if (valid) {
                    _render_primitive(cur, vb);
                    valid = false;
                }
            }
    }
}

auto _make_render_builder() -> Renderer::VertexBuilder {
    if (AdvancedRender)
        return {GL_QUADS, 3, 3, 1, 3, 1};
    return {GL_QUADS, 3, 3, 1};
}

void Chunk::build_meshes(std::array<Chunk const*, 3 * 3 * 3> neighbors) {
    auto vbs = std::array<Renderer::VertexBuilder, 2>{
        {_make_render_builder(), _make_render_builder()}
    };
    auto rd = kls::temp::make_unique<ChunkRenderData>(_coord, neighbors);
    for (auto steps = 0; steps < vbs.size(); ++steps) {
        if (MergeFace)
            _merge_face_render_chunk_pass(*rd.get(), vbs[steps], steps);
        else
            _render_chunk_pass(*rd.get(), vbs[steps], steps);
    }
    // Build new VBOs
    _meshes = {vbs[0].End(true), vbs[1].End(true)};

    // Update flags
    if (!_meshed)
        _load_anim = static_cast<float>(_coord.y() * Chunk::SIZE + Chunk::SIZE);
    _meshed = true;
    _updated = false;
}

}

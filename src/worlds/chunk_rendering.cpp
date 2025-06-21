module worlds:chunk_rendering;
import :worlds;
import std;
import types;
import math;
import debug;
import blocks;
import render;
import rendering;
import textures;
import globals;
import chunks;

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

constexpr auto coords_extend = std::array<std::array<Vec3f, 4>, 6>({
    {{{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}, // Right (Z+)
    {{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}}}, // Left (Z+)
    {{{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}}}, // Top (Z+)
    {{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}}, // Bottom (Z+)
    {{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Front (Y+)
    {{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Back (Y+)
});

constexpr auto tex_coords = std::array<std::array<Vec3f, 4>, 6>({
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Right
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Left
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Top
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Bottom
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Front
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Back
});

constexpr auto tex_coords_extend = std::array<std::array<Vec3f, 4>, 6>({
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}}}, // Right (Z+)
    {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}}}, // Left (Z+)
    {{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Top (Z+)
    {{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Bottom (Z+)
    {{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Front (Y+)
    {{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}}, // Back (Y+)
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

// One face in merge face
struct QuadPrimitive {
    int x = 0, y = 0, z = 0, length = 0, direction = 0;
    // If the vertexes have different colors (smoothed lighting), the primitive cannot connect with others.
    // This variable indicates whether the vertexes have different colors.
    bool once = false;
    // Vertex colors.
    Vec4i col = {};
    // Block ID.
    blocks::Id blk = {};
    // Texture index.
    TextureIndex tex = TextureIndex::NULLBLOCK;
};

// Before rendering, the whole chunks::Chunk and neighboring blocks are converted into this structure.
//
// * Since merge face rendering goes through the whole chunks::Chunk 12 times, it is beneficial to
//   avoid repeatedly looking up block properties in the block info registry.
// * All block properties relevant to rendering should be stored in this structure.
class BlockRenderData {
public:
    BlockRenderData() = default;
    BlockRenderData(blocks::BlockData block):
        _id(block.id) {
        auto const& info = block_info(_id);

        // Skylight has exponential falloff (mostly affects water bodies).
        auto sl = static_cast<uint8_t>(255 * std::pow(0.8f, 15.0f - static_cast<float>(block.light.sky())));

        // Blocklight has inverse-quadratic falloff.
        auto bl = static_cast<uint8_t>(255 / std::max(1, (16 - block.light.block()) * (16 - block.light.block()) / 10));

        // Temporary: mix two light levels.
        _color = info.opaque ? 0 : std::max(sl, bl);

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
    auto color() const -> int {
        return _color;
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

    auto should_render(size_t layer) -> bool {
        if (skipped())
            return false;
        if (layer == 0 && translucent())
            return false;
        if (layer == 1 && !translucent())
            return false;
        return true;
    }

    auto should_render_face(BlockRenderData neighbor, size_t layer) -> bool {
        if (neighbor.opaque())
            return false;
        if (_id == neighbor._id && _id != base_blocks().leaf)
            return false;
        return true;
    }

private:
    blocks::Id _id = {};
    std::uint8_t _color = 0;
    std::uint8_t _flags = 0;
};

// All data needed to render a chunks::Chunk.
class ChunkRenderData {
public:
    ChunkRenderData(Vec3i ccoord, std::array<chunks::Chunk const*, 3 * 3 * 3> neighbors) {
        auto index = 0uz;
        for (int x = -1; x < chunks::Chunk::SIZE + 1; x++) {
            auto rcx = x >> chunks::Chunk::SIZE_LOG;
            auto bx = static_cast<uint32_t>(x) & (chunks::Chunk::SIZE - 1);
            for (int y = -1; y < chunks::Chunk::SIZE + 1; y++) {
                auto rcy = y >> chunks::Chunk::SIZE_LOG;
                auto by = static_cast<uint32_t>(y) & (chunks::Chunk::SIZE - 1);
                for (int z = -1; z < chunks::Chunk::SIZE + 1; z++) {
                    auto rcz = z >> chunks::Chunk::SIZE_LOG;
                    auto bz = static_cast<uint32_t>(z) & (chunks::Chunk::SIZE - 1);
                    auto& block = _data[index++];
                    auto cptr = neighbors[((rcx + 1) * 3 + rcy + 1) * 3 + rcz + 1];
                    assert(cptr, "neighbors array should not contain null pointer");
                    block = cptr->block(Vec3u(bx, by, bz));
                }
            }
        }
    }

    auto block(int x, int y, int z) const -> BlockRenderData {
        return _data[((x + 1) * (chunks::Chunk::SIZE + 2) + y + 1) * (chunks::Chunk::SIZE + 2) + z + 1];
    }

private:
    std::array<BlockRenderData, (chunks::Chunk::SIZE + 2) * (chunks::Chunk::SIZE + 2) * (chunks::Chunk::SIZE + 2)>
        _data = {};
};

// The default method for rendering a block.
void _render_block(ChunkRenderData const& rd, AttribIndexBuilder& v, size_t layer, int x, int y, int z) {
    auto bl = rd.block(x, y, z);
    if (!bl.should_render(layer)) {
        return;
    }
    auto neighbors = std::array{
        rd.block(x + 1, y, z),
        rd.block(x - 1, y, z),
        rd.block(x, y + 1, z),
        rd.block(x, y - 1, z),
        rd.block(x, y, z + 1),
        rd.block(x, y, z - 1),
    };
    auto tex = TextureIndex::NULLBLOCK;
    auto col = Vec4i(0, 0, 0, 0);

    // Right face
    if (bl.should_render_face(neighbors[0], layer)) {
        if (NiceGrass && bl.id() == base_blocks().grass && rd.block(x + 1, y - 1, z).id() == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id(), 0);
        else
            tex = Textures::getTextureIndex(bl.id(), 1);
        int br = rd.block(x + 1, y, z).color();
        if (SmoothLighting) {
            col[0] = br + rd.block(x + 1, y - 1, z).color() + rd.block(x + 1, y, z + 1).color()
                   + rd.block(x + 1, y - 1, z + 1).color();
            col[1] = br + rd.block(x + 1, y - 1, z).color() + rd.block(x + 1, y, z - 1).color()
                   + rd.block(x + 1, y - 1, z - 1).color();
            col[2] = br + rd.block(x + 1, y + 1, z).color() + rd.block(x + 1, y, z - 1).color()
                   + rd.block(x + 1, y + 1, z - 1).color();
            col[3] = br + rd.block(x + 1, y + 1, z).color() + rd.block(x + 1, y, z + 1).color()
                   + rd.block(x + 1, y + 1, z + 1).color();
        } else {
            col[0] = col[1] = col[2] = col[3] = br * 4;
        }
        col = col / 4;
        if (!AdvancedRender) {
            col = col * 5 / 10;
        }
        v.material(bl.id().get());
        v.tangent(tangents[0]);
        v.bitangent(bitangents[0]);
        v.color(col[0]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[0][0]);
        v.coord(Vec3f(x, y, z) + coords[0][0]);
        v.color(col[1]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[0][1]);
        v.coord(Vec3f(x, y, z) + coords[0][1]);
        v.color(col[2]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[0][2]);
        v.coord(Vec3f(x, y, z) + coords[0][2]);
        v.color(col[3]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[0][3]);
        v.coord(Vec3f(x, y, z) + coords[0][3]);
        v.end_primitive();
    }

    // Left Face
    if (bl.should_render_face(neighbors[1], layer)) {
        if (NiceGrass && bl.id() == base_blocks().grass && rd.block(x - 1, y - 1, z).id() == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id(), 0);
        else
            tex = Textures::getTextureIndex(bl.id(), 1);
        int br = rd.block(x - 1, y, z).color();
        if (SmoothLighting) {
            col[0] = br + rd.block(x - 1, y - 1, z).color() + rd.block(x - 1, y, z - 1).color()
                   + rd.block(x - 1, y - 1, z - 1).color();
            col[1] = br + rd.block(x - 1, y - 1, z).color() + rd.block(x - 1, y, z + 1).color()
                   + rd.block(x - 1, y - 1, z + 1).color();
            col[2] = br + rd.block(x - 1, y + 1, z).color() + rd.block(x - 1, y, z + 1).color()
                   + rd.block(x - 1, y + 1, z + 1).color();
            col[3] = br + rd.block(x - 1, y + 1, z).color() + rd.block(x - 1, y, z - 1).color()
                   + rd.block(x - 1, y + 1, z - 1).color();
        } else {
            col[0] = col[1] = col[2] = col[3] = br * 4;
        }
        col = col / 4;
        if (!AdvancedRender) {
            col = col * 5 / 10;
        }
        v.material(bl.id().get());
        v.tangent(tangents[1]);
        v.bitangent(bitangents[1]);
        v.color(col[0]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[1][0]);
        v.coord(Vec3f(x, y, z) + coords[1][0]);
        v.color(col[1]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[1][1]);
        v.coord(Vec3f(x, y, z) + coords[1][1]);
        v.color(col[2]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[1][2]);
        v.coord(Vec3f(x, y, z) + coords[1][2]);
        v.color(col[3]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[1][3]);
        v.coord(Vec3f(x, y, z) + coords[1][3]);
        v.end_primitive();
    }

    // Top Face
    if (bl.should_render_face(neighbors[2], layer)) {
        tex = Textures::getTextureIndex(bl.id(), 0);
        int br = rd.block(x, y + 1, z).color();
        if (SmoothLighting) {
            col[0] = br + rd.block(x, y + 1, z + 1).color() + rd.block(x - 1, y + 1, z).color()
                   + rd.block(x - 1, y + 1, z + 1).color();
            col[1] = br + rd.block(x, y + 1, z + 1).color() + rd.block(x + 1, y + 1, z).color()
                   + rd.block(x + 1, y + 1, z + 1).color();
            col[2] = br + rd.block(x, y + 1, z - 1).color() + rd.block(x + 1, y + 1, z).color()
                   + rd.block(x + 1, y + 1, z - 1).color();
            col[3] = br + rd.block(x, y + 1, z - 1).color() + rd.block(x - 1, y + 1, z).color()
                   + rd.block(x - 1, y + 1, z - 1).color();
        } else {
            col[0] = col[1] = col[2] = col[3] = br * 4;
        }
        col = col / 4;
        v.material(bl.id().get());
        v.tangent(tangents[2]);
        v.bitangent(bitangents[2]);
        v.color(col[0]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[2][0]);
        v.coord(Vec3f(x, y, z) + coords[2][0]);
        v.color(col[1]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[2][1]);
        v.coord(Vec3f(x, y, z) + coords[2][1]);
        v.color(col[2]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[2][2]);
        v.coord(Vec3f(x, y, z) + coords[2][2]);
        v.color(col[3]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[2][3]);
        v.coord(Vec3f(x, y, z) + coords[2][3]);
        v.end_primitive();
    }

    // Bottom Face
    if (bl.should_render_face(neighbors[3], layer)) {
        tex = Textures::getTextureIndex(bl.id(), 2);
        int br = rd.block(x, y - 1, z).color();
        if (SmoothLighting) {
            col[0] = br + rd.block(x, y - 1, z - 1).color() + rd.block(x - 1, y - 1, z).color()
                   + rd.block(x - 1, y - 1, z - 1).color();
            col[1] = br + rd.block(x, y - 1, z - 1).color() + rd.block(x + 1, y - 1, z).color()
                   + rd.block(x + 1, y - 1, z - 1).color();
            col[2] = br + rd.block(x, y - 1, z + 1).color() + rd.block(x + 1, y - 1, z).color()
                   + rd.block(x + 1, y - 1, z + 1).color();
            col[3] = br + rd.block(x, y - 1, z + 1).color() + rd.block(x - 1, y - 1, z).color()
                   + rd.block(x - 1, y - 1, z + 1).color();
        } else {
            col[0] = col[1] = col[2] = col[3] = br * 4;
        }
        col = col / 4;
        v.material(bl.id().get());
        v.tangent(tangents[3]);
        v.bitangent(bitangents[3]);
        v.color(col[0]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[3][0]);
        v.coord(Vec3f(x, y, z) + coords[3][0]);
        v.color(col[1]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[3][1]);
        v.coord(Vec3f(x, y, z) + coords[3][1]);
        v.color(col[2]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[3][2]);
        v.coord(Vec3f(x, y, z) + coords[3][2]);
        v.color(col[3]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[3][3]);
        v.coord(Vec3f(x, y, z) + coords[3][3]);
        v.end_primitive();
    }

    // Front Face
    if (bl.should_render_face(neighbors[4], layer)) {
        if (NiceGrass && bl.id() == base_blocks().grass && rd.block(x, y - 1, z + 1).id() == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id(), 0);
        else
            tex = Textures::getTextureIndex(bl.id(), 1);
        int br = rd.block(x, y, z + 1).color();
        if (SmoothLighting) {
            col[0] = br + rd.block(x, y - 1, z + 1).color() + rd.block(x - 1, y, z + 1).color()
                   + rd.block(x - 1, y - 1, z + 1).color();
            col[1] = br + rd.block(x, y - 1, z + 1).color() + rd.block(x + 1, y, z + 1).color()
                   + rd.block(x + 1, y - 1, z + 1).color();
            col[2] = br + rd.block(x, y + 1, z + 1).color() + rd.block(x + 1, y, z + 1).color()
                   + rd.block(x + 1, y + 1, z + 1).color();
            col[3] = br + rd.block(x, y + 1, z + 1).color() + rd.block(x - 1, y, z + 1).color()
                   + rd.block(x - 1, y + 1, z + 1).color();
        } else {
            col[0] = col[1] = col[2] = col[3] = br * 4;
        }
        col = col / 4;
        if (!AdvancedRender) {
            col = col * 2 / 10;
        }
        v.material(bl.id().get());
        v.tangent(tangents[4]);
        v.bitangent(bitangents[4]);
        v.color(col[0]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[4][0]);
        v.coord(Vec3f(x, y, z) + coords[4][0]);
        v.color(col[1]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[4][1]);
        v.coord(Vec3f(x, y, z) + coords[4][1]);
        v.color(col[2]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[4][2]);
        v.coord(Vec3f(x, y, z) + coords[4][2]);
        v.color(col[3]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[4][3]);
        v.coord(Vec3f(x, y, z) + coords[4][3]);
        v.end_primitive();
    }

    // Back Face
    if (bl.should_render_face(neighbors[5], layer)) {
        if (NiceGrass && bl.id() == base_blocks().grass && rd.block(x, y - 1, z - 1).id() == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id(), 0);
        else
            tex = Textures::getTextureIndex(bl.id(), 1);
        int br = rd.block(x, y, z - 1).color();
        if (SmoothLighting) {
            col[0] = br + rd.block(x, y - 1, z - 1).color() + rd.block(x + 1, y, z - 1).color()
                   + rd.block(x + 1, y - 1, z - 1).color();
            col[1] = br + rd.block(x, y - 1, z - 1).color() + rd.block(x - 1, y, z - 1).color()
                   + rd.block(x - 1, y - 1, z - 1).color();
            col[2] = br + rd.block(x, y + 1, z - 1).color() + rd.block(x - 1, y, z - 1).color()
                   + rd.block(x - 1, y + 1, z - 1).color();
            col[3] = br + rd.block(x, y + 1, z - 1).color() + rd.block(x + 1, y, z - 1).color()
                   + rd.block(x + 1, y + 1, z - 1).color();
        } else {
            col[0] = col[1] = col[2] = col[3] = br * 4;
        }
        col = col / 4;
        if (!AdvancedRender) {
            col = col * 2 / 10;
        }
        v.material(bl.id().get());
        v.tangent(tangents[5]);
        v.bitangent(bitangents[5]);
        v.color(col[0]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[5][0]);
        v.coord(Vec3f(x, y, z) + coords[5][0]);
        v.color(col[1]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[5][1]);
        v.coord(Vec3f(x, y, z) + coords[5][1]);
        v.color(col[2]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[5][2]);
        v.coord(Vec3f(x, y, z) + coords[5][2]);
        v.color(col[3]);
        v.tex_coord(Vec3f(0, 0, tex) + tex_coords[5][3]);
        v.coord(Vec3f(x, y, z) + coords[5][3]);
        v.end_primitive();
    }
}

// The merge face rendering method for a primitive (adjacent block faces).
void _render_primitive(QuadPrimitive const& p, AttribIndexBuilder& v) {
    auto extend = static_cast<float>(p.length);
    auto col = p.col / 4;
    switch (p.direction) {
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
    v.material(p.blk.get());
    v.tangent(tangents[p.direction]);
    v.bitangent(bitangents[p.direction]);
    v.color(col[0]);
    v.tex_coord(Vec3f(0, 0, p.tex) + tex_coords[p.direction][0] + tex_coords_extend[p.direction][0] * extend);
    v.coord(Vec3f(p.x, p.y, p.z) + coords[p.direction][0] + coords_extend[p.direction][0] * extend);
    v.color(col[1]);
    v.tex_coord(Vec3f(0, 0, p.tex) + tex_coords[p.direction][1] + tex_coords_extend[p.direction][1] * extend);
    v.coord(Vec3f(p.x, p.y, p.z) + coords[p.direction][1] + coords_extend[p.direction][1] * extend);
    v.color(col[2]);
    v.tex_coord(Vec3f(0, 0, p.tex) + tex_coords[p.direction][2] + tex_coords_extend[p.direction][2] * extend);
    v.coord(Vec3f(p.x, p.y, p.z) + coords[p.direction][2] + coords_extend[p.direction][2] * extend);
    v.color(col[3]);
    v.tex_coord(Vec3f(0, 0, p.tex) + tex_coords[p.direction][3] + tex_coords_extend[p.direction][3] * extend);
    v.coord(Vec3f(p.x, p.y, p.z) + coords[p.direction][3] + coords_extend[p.direction][3] * extend);
    v.end_primitive();
}

// The default method for rendering a chunks::Chunk.
void _render_chunk(ChunkRenderData const& rd, AttribIndexBuilder& v, size_t layer) {
    for (auto x = 0; x < chunks::Chunk::SIZE; x++)
        for (auto y = 0; y < chunks::Chunk::SIZE; y++)
            for (auto z = 0; z < chunks::Chunk::SIZE; z++) {
                _render_block(rd, v, layer, x, y, z);
            }
}

// The merge face rendering method for a chunks::Chunk.
//
// * For faces orienting X+ (d = 0) or X- (d = 1), the merge direction is Z+;
// * For faces orienting Y+ (d = 2) or Y- (d = 3), the merge direction is Z+;
// * For faces orienting Z+ (d = 4) or Z- (d = 5), the merge direction is Y+.
//
// The merge directions are determined by the fact that block render data are stored in a
// X-Y-Z-major order, which means blocks adjacent in the Z direction are stored consecutively.
// This probably allows for better cache hit rates.
void _merge_face_render_chunk(ChunkRenderData const& rd, AttribIndexBuilder& v, size_t layer) {
    // For each direction
    for (auto d = 0; d < 6; d++) {
        // Render current direction
        for (auto i = 0; i < chunks::Chunk::SIZE; i++)
            for (auto j = 0; j < chunks::Chunk::SIZE; j++) {
                QuadPrimitive cur;
                bool valid = false;
                // Linear merge
                for (int k = 0; k < chunks::Chunk::SIZE; k++) {
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
                    if (!bl.should_render(layer) || !bl.should_render_face(rd.block(x + dx, y + dy, z + dz), layer)) {
                        if (valid) {
                            _render_primitive(cur, v);
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
                    auto br = 0;
                    auto col = Vec4i(0, 0, 0, 0);
                    switch (d) {
                        case 0:
                            if (NiceGrass && bl.id() == base_blocks().grass
                                && rd.block(x + 1, y - 1, z).id() == base_blocks().grass)
                                tex = Textures::getTextureIndex(bl.id(), 0);
                            br = rd.block(x + 1, y, z).color();
                            if (SmoothLighting) {
                                col[0] = br + rd.block(x + 1, y - 1, z).color() + rd.block(x + 1, y, z + 1).color()
                                       + rd.block(x + 1, y - 1, z + 1).color();
                                col[1] = br + rd.block(x + 1, y - 1, z).color() + rd.block(x + 1, y, z - 1).color()
                                       + rd.block(x + 1, y - 1, z - 1).color();
                                col[2] = br + rd.block(x + 1, y + 1, z).color() + rd.block(x + 1, y, z - 1).color()
                                       + rd.block(x + 1, y + 1, z - 1).color();
                                col[3] = br + rd.block(x + 1, y + 1, z).color() + rd.block(x + 1, y, z + 1).color()
                                       + rd.block(x + 1, y + 1, z + 1).color();
                            } else {
                                col[0] = col[1] = col[2] = col[3] = br * 4;
                            }
                            break;
                        case 1:
                            if (NiceGrass && bl.id() == base_blocks().grass
                                && rd.block(x - 1, y - 1, z).id() == base_blocks().grass)
                                tex = Textures::getTextureIndex(bl.id(), 0);
                            br = rd.block(x - 1, y, z).color();
                            if (SmoothLighting) {
                                col[0] = br + rd.block(x - 1, y - 1, z).color() + rd.block(x - 1, y, z - 1).color()
                                       + rd.block(x - 1, y - 1, z - 1).color();
                                col[1] = br + rd.block(x - 1, y - 1, z).color() + rd.block(x - 1, y, z + 1).color()
                                       + rd.block(x - 1, y - 1, z + 1).color();
                                col[2] = br + rd.block(x - 1, y + 1, z).color() + rd.block(x - 1, y, z + 1).color()
                                       + rd.block(x - 1, y + 1, z + 1).color();
                                col[3] = br + rd.block(x - 1, y + 1, z).color() + rd.block(x - 1, y, z - 1).color()
                                       + rd.block(x - 1, y + 1, z - 1).color();
                            } else {
                                col[0] = col[1] = col[2] = col[3] = br * 4;
                            }
                            break;
                        case 2:
                            br = rd.block(x, y + 1, z).color();
                            if (SmoothLighting) {
                                col[0] = br + rd.block(x - 1, y + 1, z).color() + rd.block(x, y + 1, z + 1).color()
                                       + rd.block(x - 1, y + 1, z + 1).color();
                                col[1] = br + rd.block(x + 1, y + 1, z).color() + rd.block(x, y + 1, z + 1).color()
                                       + rd.block(x + 1, y + 1, z + 1).color();
                                col[2] = br + rd.block(x + 1, y + 1, z).color() + rd.block(x, y + 1, z - 1).color()
                                       + rd.block(x + 1, y + 1, z - 1).color();
                                col[3] = br + rd.block(x - 1, y + 1, z).color() + rd.block(x, y + 1, z - 1).color()
                                       + rd.block(x - 1, y + 1, z - 1).color();
                            } else {
                                col[0] = col[1] = col[2] = col[3] = br * 4;
                            }
                            break;
                        case 3:
                            br = rd.block(x, y - 1, z).color();
                            if (SmoothLighting) {
                                col[0] = br + rd.block(x - 1, y - 1, z).color() + rd.block(x, y - 1, z - 1).color()
                                       + rd.block(x - 1, y - 1, z - 1).color();
                                col[1] = br + rd.block(x + 1, y - 1, z).color() + rd.block(x, y - 1, z - 1).color()
                                       + rd.block(x + 1, y - 1, z - 1).color();
                                col[2] = br + rd.block(x + 1, y - 1, z).color() + rd.block(x, y - 1, z + 1).color()
                                       + rd.block(x + 1, y - 1, z + 1).color();
                                col[3] = br + rd.block(x - 1, y - 1, z).color() + rd.block(x, y - 1, z + 1).color()
                                       + rd.block(x - 1, y - 1, z + 1).color();
                            } else {
                                col[0] = col[1] = col[2] = col[3] = br * 4;
                            }
                            break;
                        case 4:
                            if (NiceGrass && bl.id() == base_blocks().grass
                                && rd.block(x, y - 1, z + 1).id() == base_blocks().grass)
                                tex = Textures::getTextureIndex(bl.id(), 0);
                            br = rd.block(x, y, z + 1).color();
                            if (SmoothLighting) {
                                col[0] = br + rd.block(x - 1, y, z + 1).color() + rd.block(x, y - 1, z + 1).color()
                                       + rd.block(x - 1, y - 1, z + 1).color();
                                col[1] = br + rd.block(x + 1, y, z + 1).color() + rd.block(x, y - 1, z + 1).color()
                                       + rd.block(x + 1, y - 1, z + 1).color();
                                col[2] = br + rd.block(x + 1, y, z + 1).color() + rd.block(x, y + 1, z + 1).color()
                                       + rd.block(x + 1, y + 1, z + 1).color();
                                col[3] = br + rd.block(x - 1, y, z + 1).color() + rd.block(x, y + 1, z + 1).color()
                                       + rd.block(x - 1, y + 1, z + 1).color();
                            } else {
                                col[0] = col[1] = col[2] = col[3] = br * 4;
                            }
                            break;
                        case 5:
                            if (NiceGrass && bl.id() == base_blocks().grass
                                && rd.block(x, y - 1, z - 1).id() == base_blocks().grass)
                                tex = Textures::getTextureIndex(bl.id(), 0);
                            br = rd.block(x, y, z - 1).color();
                            if (SmoothLighting) {
                                col[0] = br + rd.block(x + 1, y, z - 1).color() + rd.block(x, y - 1, z - 1).color()
                                       + rd.block(x + 1, y - 1, z - 1).color();
                                col[1] = br + rd.block(x - 1, y, z - 1).color() + rd.block(x, y - 1, z - 1).color()
                                       + rd.block(x - 1, y - 1, z - 1).color();
                                col[2] = br + rd.block(x - 1, y, z - 1).color() + rd.block(x, y + 1, z - 1).color()
                                       + rd.block(x - 1, y + 1, z - 1).color();
                                col[3] = br + rd.block(x + 1, y, z - 1).color() + rd.block(x, y + 1, z - 1).color()
                                       + rd.block(x + 1, y + 1, z - 1).color();
                            } else {
                                col[0] = col[1] = col[2] = col[3] = br * 4;
                            }
                            break;
                    }
                    // Render
                    bool once = col[0] != col[1] || col[1] != col[2] || col[2] != col[3];
                    if (valid) {
                        if (once || cur.once || bl.id() != cur.blk || tex != cur.tex || col[0] != cur.col[0]) {
                            _render_primitive(cur, v);
                            cur.x = x;
                            cur.y = y;
                            cur.z = z;
                            cur.length = 0;
                            cur.direction = d;
                            cur.once = once;
                            cur.blk = bl.id();
                            cur.tex = tex;
                            cur.col[0] = col[0];
                            cur.col[1] = col[1];
                            cur.col[2] = col[2];
                            cur.col[3] = col[3];
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
                        cur.col[0] = col[0];
                        cur.col[1] = col[1];
                        cur.col[2] = col[2];
                        cur.col[3] = col[3];
                    }
                }
                if (valid) {
                    _render_primitive(cur, v);
                    valid = false;
                }
            }
    }
}

void worlds::RenderData::build_meshes(std::array<chunks::Chunk const*, 3 * 3 * 3> neighbors) {
    // Build new VBOs
    auto rd = ChunkRenderData(_refer->coord(), neighbors);
    auto builders = std::array{
        AttribIndexBuilder(),
        AttribIndexBuilder(),
    };
    for (auto layer = 0; layer < 2uz; layer++) {
        if (MergeFace) {
            _merge_face_render_chunk(rd, builders[layer], layer);
        } else {
            _render_chunk(rd, builders[layer], layer);
        }
    }
    _meshes = {
        VertexArray::create(builders[0], VertexArray::Primitive::TRIANGLE_FAN),
        VertexArray::create(builders[1], VertexArray::Primitive::TRIANGLE_FAN),
    };

    // Update flags
    if (!_meshed) {
        _load_anim = static_cast<float>(_refer->coord().y() * chunks::Chunk::SIZE + chunks::Chunk::SIZE);
    }
    _meshed = true;
    _refer->clear_updated();
}

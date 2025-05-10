module worlds:chunk_rendering;
import :worlds;
import std;
import types;
import math;
import debug;
import blocks;
import render;
import textures;
import globals;
import chunks;

namespace spec = render::attrib_layout::spec;
using render::VertexArray;
using AttribIndexBuilder = render::AttribIndexBuilder<
    spec::Coord<spec::Vec3f>,
    spec::TexCoord<spec::Vec3f>,
    spec::Color<spec::Vec3u8>,
    spec::Normal<spec::Vec3i8>,
    spec::Material<spec::UInt16>>;

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
    std::uint8_t _light = 0;
    std::uint8_t _flags = 0;
};

// Temporary: maximum value obtained after mixing two light levels.
constexpr auto MAX_LIGHT = 15;

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
        int br = rd.block(x + 1, y, z).light();
        if (SmoothLighting) {
            col[0] = br + rd.block(x + 1, y - 1, z).light() + rd.block(x + 1, y, z - 1).light()
                   + rd.block(x + 1, y - 1, z - 1).light();
            col[1] = br + rd.block(x + 1, y + 1, z).light() + rd.block(x + 1, y, z - 1).light()
                   + rd.block(x + 1, y + 1, z - 1).light();
            col[2] = br + rd.block(x + 1, y + 1, z).light() + rd.block(x + 1, y, z + 1).light()
                   + rd.block(x + 1, y + 1, z + 1).light();
            col[3] = br + rd.block(x + 1, y - 1, z).light() + rd.block(x + 1, y, z + 1).light()
                   + rd.block(x + 1, y - 1, z + 1).light();
        } else {
            col[0] = col[1] = col[2] = col[3] = br * 4;
        }
        col = col * 255 / (MAX_LIGHT * 4);
        if (!AdvancedRender) {
            col = col * 7 / 10;
        }
        v.material(bl.id().get());
        v.normal({1, 0, 0});
        v.color(col[0]);
        v.tex_coord({1, 0, static_cast<uint16_t>(tex)});
        v.coord({x + 1, y, z});
        v.color(col[1]);
        v.tex_coord({1, 1, static_cast<uint16_t>(tex)});
        v.coord({x + 1, y + 1, z});
        v.color(col[2]);
        v.tex_coord({0, 1, static_cast<uint16_t>(tex)});
        v.coord({x + 1, y + 1, z + 1});
        v.color(col[3]);
        v.tex_coord({0, 0, static_cast<uint16_t>(tex)});
        v.coord({x + 1, y, z + 1});
        v.end_primitive();
    }

    // Left Face
    if (bl.should_render_face(neighbors[1], layer)) {
        if (NiceGrass && bl.id() == base_blocks().grass && rd.block(x - 1, y - 1, z).id() == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id(), 0);
        else
            tex = Textures::getTextureIndex(bl.id(), 1);
        int br = rd.block(x - 1, y, z).light();
        if (SmoothLighting) {
            col[0] = br + rd.block(x - 1, y - 1, z).light() + rd.block(x - 1, y, z - 1).light()
                   + rd.block(x - 1, y - 1, z - 1).light();
            col[1] = br + rd.block(x - 1, y - 1, z).light() + rd.block(x - 1, y, z + 1).light()
                   + rd.block(x - 1, y - 1, z + 1).light();
            col[2] = br + rd.block(x - 1, y + 1, z).light() + rd.block(x - 1, y, z + 1).light()
                   + rd.block(x - 1, y + 1, z + 1).light();
            col[3] = br + rd.block(x - 1, y + 1, z).light() + rd.block(x - 1, y, z - 1).light()
                   + rd.block(x - 1, y + 1, z - 1).light();
        } else {
            col[0] = col[1] = col[2] = col[3] = br * 4;
        }
        col = col * 255 / (MAX_LIGHT * 4);
        if (!AdvancedRender) {
            col = col * 7 / 10;
        }
        v.material(bl.id().get());
        v.normal({-1, 0, 0});
        v.color(col[0]);
        v.tex_coord({0, 0, static_cast<uint16_t>(tex)});
        v.coord({x, y, z});
        v.color(col[1]);
        v.tex_coord({1, 0, static_cast<uint16_t>(tex)});
        v.coord({x, y, z + 1});
        v.color(col[2]);
        v.tex_coord({1, 1, static_cast<uint16_t>(tex)});
        v.coord({x, y + 1, z + 1});
        v.color(col[3]);
        v.tex_coord({0, 1, static_cast<uint16_t>(tex)});
        v.coord({x, y + 1, z});
        v.end_primitive();
    }

    // Top Face
    if (bl.should_render_face(neighbors[2], layer)) {
        tex = Textures::getTextureIndex(bl.id(), 0);
        int br = rd.block(x, y + 1, z).light();
        if (SmoothLighting) {
            col[0] = br + rd.block(x, y + 1, z - 1).light() + rd.block(x - 1, y + 1, z).light()
                   + rd.block(x - 1, y + 1, z - 1).light();
            col[1] = br + rd.block(x, y + 1, z + 1).light() + rd.block(x - 1, y + 1, z).light()
                   + rd.block(x - 1, y + 1, z + 1).light();
            col[2] = br + rd.block(x, y + 1, z + 1).light() + rd.block(x + 1, y + 1, z).light()
                   + rd.block(x + 1, y + 1, z + 1).light();
            col[3] = br + rd.block(x, y + 1, z - 1).light() + rd.block(x + 1, y + 1, z).light()
                   + rd.block(x + 1, y + 1, z - 1).light();
        } else {
            col[0] = col[1] = col[2] = col[3] = br * 4;
        }
        col = col * 255 / (MAX_LIGHT * 4);
        v.material(bl.id().get());
        v.normal({0, 1, 0});
        v.color(col[0]);
        v.tex_coord({0, 1, static_cast<uint16_t>(tex)});
        v.coord({x, y + 1, z});
        v.color(col[1]);
        v.tex_coord({0, 0, static_cast<uint16_t>(tex)});
        v.coord({x, y + 1, z + 1});
        v.color(col[2]);
        v.tex_coord({1, 0, static_cast<uint16_t>(tex)});
        v.coord({x + 1, y + 1, z + 1});
        v.color(col[3]);
        v.tex_coord({1, 1, static_cast<uint16_t>(tex)});
        v.coord({x + 1, y + 1, z});
        v.end_primitive();
    }

    // Bottom Face
    if (bl.should_render_face(neighbors[3], layer)) {
        tex = Textures::getTextureIndex(bl.id(), 2);
        int br = rd.block(x, y - 1, z).light();
        if (SmoothLighting) {
            col[0] = br + rd.block(x, y - 1, z - 1).light() + rd.block(x - 1, y - 1, z).light()
                   + rd.block(x - 1, y - 1, z - 1).light();
            col[1] = br + rd.block(x, y - 1, z - 1).light() + rd.block(x + 1, y - 1, z).light()
                   + rd.block(x + 1, y - 1, z - 1).light();
            col[2] = br + rd.block(x, y - 1, z + 1).light() + rd.block(x + 1, y - 1, z).light()
                   + rd.block(x + 1, y - 1, z + 1).light();
            col[3] = br + rd.block(x, y - 1, z + 1).light() + rd.block(x - 1, y - 1, z).light()
                   + rd.block(x - 1, y - 1, z + 1).light();
        } else {
            col[0] = col[1] = col[2] = col[3] = br * 4;
        }
        col = col * 255 / (MAX_LIGHT * 4);
        v.material(bl.id().get());
        v.normal({0, -1, 0});
        v.color(col[0]);
        v.tex_coord({1, 1, static_cast<uint16_t>(tex)});
        v.coord({x, y, z});
        v.color(col[1]);
        v.tex_coord({0, 1, static_cast<uint16_t>(tex)});
        v.coord({x + 1, y, z});
        v.color(col[2]);
        v.tex_coord({0, 0, static_cast<uint16_t>(tex)});
        v.coord({x + 1, y, z + 1});
        v.color(col[3]);
        v.tex_coord({1, 0, static_cast<uint16_t>(tex)});
        v.coord({x, y, z + 1});
        v.end_primitive();
    }

    // Front Face
    if (bl.should_render_face(neighbors[4], layer)) {
        if (NiceGrass && bl.id() == base_blocks().grass && rd.block(x, y - 1, z + 1).id() == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id(), 0);
        else
            tex = Textures::getTextureIndex(bl.id(), 1);
        int br = rd.block(x, y, z + 1).light();
        if (SmoothLighting) {
            col[0] = br + rd.block(x, y - 1, z + 1).light() + rd.block(x - 1, y, z + 1).light()
                   + rd.block(x - 1, y - 1, z + 1).light();
            col[1] = br + rd.block(x, y - 1, z + 1).light() + rd.block(x + 1, y, z + 1).light()
                   + rd.block(x + 1, y - 1, z + 1).light();
            col[2] = br + rd.block(x, y + 1, z + 1).light() + rd.block(x + 1, y, z + 1).light()
                   + rd.block(x + 1, y + 1, z + 1).light();
            col[3] = br + rd.block(x, y + 1, z + 1).light() + rd.block(x - 1, y, z + 1).light()
                   + rd.block(x - 1, y + 1, z + 1).light();
        } else {
            col[0] = col[1] = col[2] = col[3] = br * 4;
        }
        col = col * 255 / (MAX_LIGHT * 4);
        if (!AdvancedRender) {
            col = col * 5 / 10;
        }
        v.material(bl.id().get());
        v.normal({0, 0, 1});
        v.color(col[0]);
        v.tex_coord({0, 0, static_cast<uint16_t>(tex)});
        v.coord({x, y, z + 1});
        v.color(col[1]);
        v.tex_coord({1, 0, static_cast<uint16_t>(tex)});
        v.coord({x + 1, y, z + 1});
        v.color(col[2]);
        v.tex_coord({1, 1, static_cast<uint16_t>(tex)});
        v.coord({x + 1, y + 1, z + 1});
        v.color(col[3]);
        v.tex_coord({0, 1, static_cast<uint16_t>(tex)});
        v.coord({x, y + 1, z + 1});
        v.end_primitive();
    }

    // Back Face
    if (bl.should_render_face(neighbors[5], layer)) {
        if (NiceGrass && bl.id() == base_blocks().grass && rd.block(x, y - 1, z - 1).id() == base_blocks().grass)
            tex = Textures::getTextureIndex(bl.id(), 0);
        else
            tex = Textures::getTextureIndex(bl.id(), 1);
        int br = rd.block(x, y, z - 1).light();
        if (SmoothLighting) {
            col[0] = br + rd.block(x, y - 1, z - 1).light() + rd.block(x - 1, y, z - 1).light()
                   + rd.block(x - 1, y - 1, z - 1).light();
            col[1] = br + rd.block(x, y + 1, z - 1).light() + rd.block(x - 1, y, z - 1).light()
                   + rd.block(x - 1, y + 1, z - 1).light();
            col[2] = br + rd.block(x, y + 1, z - 1).light() + rd.block(x + 1, y, z - 1).light()
                   + rd.block(x + 1, y + 1, z - 1).light();
            col[3] = br + rd.block(x, y - 1, z - 1).light() + rd.block(x + 1, y, z - 1).light()
                   + rd.block(x + 1, y - 1, z - 1).light();
        } else {
            col[0] = col[1] = col[2] = col[3] = br * 4;
        }
        col = col * 255 / (MAX_LIGHT * 4);
        if (!AdvancedRender) {
            col = col * 5 / 10;
        }
        v.material(bl.id().get());
        v.normal({0, 0, -1});
        v.color(col[0]);
        v.tex_coord({1, 0, static_cast<uint16_t>(tex)});
        v.coord({x, y, z});
        v.color(col[1]);
        v.tex_coord({1, 1, static_cast<uint16_t>(tex)});
        v.coord({x, y + 1, z});
        v.color(col[2]);
        v.tex_coord({0, 1, static_cast<uint16_t>(tex)});
        v.coord({x + 1, y + 1, z});
        v.color(col[3]);
        v.tex_coord({0, 0, static_cast<uint16_t>(tex)});
        v.coord({x + 1, y, z});
        v.end_primitive();
    }
}

// The merge face rendering method for a primitive (adjacent block faces).
void _render_primitive(QuadPrimitive const& p, AttribIndexBuilder& v) {
    auto col = p.col * 255 / (MAX_LIGHT * 4);
    auto x = p.x, y = p.y, z = p.z, length = p.length;

    v.material(p.blk.get());
    v.tex_coord({0, 0, static_cast<uint16_t>(p.tex)});

    switch (p.direction) {
        case 0:
            if (!AdvancedRender) {
                col = col * 7 / 10;
            }
            v.normal({1, 0, 0});
            v.color(col[0]);
            v.tex_coord({0, 0, static_cast<uint16_t>(p.tex)});
            v.coord({x + 1, y, z});
            v.color(col[1]);
            v.tex_coord({0, 1, static_cast<uint16_t>(p.tex)});
            v.coord({x + 1, y + 1, z});
            v.color(col[2]);
            v.tex_coord({length + 1, 1, static_cast<uint16_t>(p.tex)});
            v.coord({x + 1, y + 1, z + length + 1});
            v.color(col[3]);
            v.tex_coord({length + 1, 0, static_cast<uint16_t>(p.tex)});
            v.coord({x + 1, y, z + length + 1});
            v.end_primitive();
            break;
        case 1:
            if (!AdvancedRender) {
                col = col * 7 / 10;
            }
            v.normal({-1, 0, 0});
            v.color(col[0]);
            v.tex_coord({0, 1, static_cast<uint16_t>(p.tex)});
            v.coord({x, y + 1, z});
            v.color(col[1]);
            v.tex_coord({0, 0, static_cast<uint16_t>(p.tex)});
            v.coord({x, y, z});
            v.color(col[2]);
            v.tex_coord({length + 1, 0, static_cast<uint16_t>(p.tex)});
            v.coord({x, y, z + length + 1});
            v.color(col[3]);
            v.tex_coord({length + 1, 1, static_cast<uint16_t>(p.tex)});
            v.coord({x, y + 1, z + length + 1});
            v.end_primitive();
            break;
        case 2:
            v.normal({0, 1, 0});
            v.color(col[0]);
            v.tex_coord({0, 0, static_cast<uint16_t>(p.tex)});
            v.coord({x + 1, y + 1, z});
            v.color(col[1]);
            v.tex_coord({0, 1, static_cast<uint16_t>(p.tex)});
            v.coord({x, y + 1, z});
            v.color(col[2]);
            v.tex_coord({length + 1, 1, static_cast<uint16_t>(p.tex)});
            v.coord({x, y + 1, z + length + 1});
            v.color(col[3]);
            v.tex_coord({length + 1, 0, static_cast<uint16_t>(p.tex)});
            v.coord({x + 1, y + 1, z + length + 1});
            v.end_primitive();
            break;
        case 3:
            v.normal({0, -1, 0});
            v.color(col[0]);
            v.tex_coord({0, 0, static_cast<uint16_t>(p.tex)});
            v.coord({x, y, z});
            v.color(col[1]);
            v.tex_coord({0, 1, static_cast<uint16_t>(p.tex)});
            v.coord({x + 1, y, z});
            v.color(col[2]);
            v.tex_coord({length + 1, 1, static_cast<uint16_t>(p.tex)});
            v.coord({x + 1, y, z + length + 1});
            v.color(col[3]);
            v.tex_coord({length + 1, 0, static_cast<uint16_t>(p.tex)});
            v.coord({x, y, z + length + 1});
            v.end_primitive();
            break;
        case 4:
            if (!AdvancedRender) {
                col = col * 5 / 10;
            }
            v.normal({0, 0, 1});
            v.color(col[0]);
            v.tex_coord({0, length + 1, static_cast<uint16_t>(p.tex)});
            v.coord({x, y + length + 1, z + 1});
            v.color(col[1]);
            v.tex_coord({0, 0, static_cast<uint16_t>(p.tex)});
            v.coord({x, y, z + 1});
            v.color(col[2]);
            v.tex_coord({1, 0, static_cast<uint16_t>(p.tex)});
            v.coord({x + 1, y, z + 1});
            v.color(col[3]);
            v.tex_coord({1, length + 1, static_cast<uint16_t>(p.tex)});
            v.coord({x + 1, y + length + 1, z + 1});
            v.end_primitive();
            break;
        case 5:
            if (!AdvancedRender) {
                col = col * 5 / 10;
            }
            v.normal({0, 0, -1});
            v.color(col[0]);
            v.tex_coord({0, 0, static_cast<uint16_t>(p.tex)});
            v.coord({x, y, z});
            v.color(col[1]);
            v.tex_coord({0, length + 1, static_cast<uint16_t>(p.tex)});
            v.coord({x, y + length + 1, z});
            v.color(col[2]);
            v.tex_coord({1, length + 1, static_cast<uint16_t>(p.tex)});
            v.coord({x + 1, y + length + 1, z});
            v.color(col[3]);
            v.tex_coord({1, 0, static_cast<uint16_t>(p.tex)});
            v.coord({x + 1, y, z});
            v.end_primitive();
            break;
    }
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
                            br = rd.block(x + 1, y, z).light();
                            if (SmoothLighting) {
                                col[0] = br + rd.block(x + 1, y - 1, z).light() + rd.block(x + 1, y, z - 1).light()
                                       + rd.block(x + 1, y - 1, z - 1).light();
                                col[1] = br + rd.block(x + 1, y + 1, z).light() + rd.block(x + 1, y, z - 1).light()
                                       + rd.block(x + 1, y + 1, z - 1).light();
                                col[2] = br + rd.block(x + 1, y + 1, z).light() + rd.block(x + 1, y, z + 1).light()
                                       + rd.block(x + 1, y + 1, z + 1).light();
                                col[3] = br + rd.block(x + 1, y - 1, z).light() + rd.block(x + 1, y, z + 1).light()
                                       + rd.block(x + 1, y - 1, z + 1).light();
                            } else {
                                col[0] = col[1] = col[2] = col[3] = br * 4;
                            }
                            break;
                        case 1:
                            if (NiceGrass && bl.id() == base_blocks().grass
                                && rd.block(x - 1, y - 1, z).id() == base_blocks().grass)
                                tex = Textures::getTextureIndex(bl.id(), 0);
                            br = rd.block(x - 1, y, z).light();
                            if (SmoothLighting) {
                                col[0] = br + rd.block(x - 1, y + 1, z).light() + rd.block(x - 1, y, z - 1).light()
                                       + rd.block(x - 1, y + 1, z - 1).light();
                                col[1] = br + rd.block(x - 1, y - 1, z).light() + rd.block(x - 1, y, z - 1).light()
                                       + rd.block(x - 1, y - 1, z - 1).light();
                                col[2] = br + rd.block(x - 1, y - 1, z).light() + rd.block(x - 1, y, z + 1).light()
                                       + rd.block(x - 1, y - 1, z + 1).light();
                                col[3] = br + rd.block(x - 1, y + 1, z).light() + rd.block(x - 1, y, z + 1).light()
                                       + rd.block(x - 1, y + 1, z + 1).light();
                            } else {
                                col[0] = col[1] = col[2] = col[3] = br * 4;
                            }
                            break;
                        case 2:
                            br = rd.block(x, y + 1, z).light();
                            if (SmoothLighting) {
                                col[0] = br + rd.block(x + 1, y + 1, z).light() + rd.block(x, y + 1, z - 1).light()
                                       + rd.block(x + 1, y + 1, z - 1).light();
                                col[1] = br + rd.block(x - 1, y + 1, z).light() + rd.block(x, y + 1, z - 1).light()
                                       + rd.block(x - 1, y + 1, z - 1).light();
                                col[2] = br + rd.block(x - 1, y + 1, z).light() + rd.block(x, y + 1, z + 1).light()
                                       + rd.block(x - 1, y + 1, z + 1).light();
                                col[3] = br + rd.block(x + 1, y + 1, z).light() + rd.block(x, y + 1, z + 1).light()
                                       + rd.block(x + 1, y + 1, z + 1).light();
                            } else {
                                col[0] = col[1] = col[2] = col[3] = br * 4;
                            }
                            break;
                        case 3:
                            br = rd.block(x, y - 1, z).light();
                            if (SmoothLighting) {
                                col[0] = br + rd.block(x - 1, y - 1, z).light() + rd.block(x, y - 1, z - 1).light()
                                       + rd.block(x - 1, y - 1, z - 1).light();
                                col[1] = br + rd.block(x + 1, y - 1, z).light() + rd.block(x, y - 1, z - 1).light()
                                       + rd.block(x + 1, y - 1, z - 1).light();
                                col[2] = br + rd.block(x + 1, y - 1, z).light() + rd.block(x, y - 1, z + 1).light()
                                       + rd.block(x + 1, y - 1, z + 1).light();
                                col[3] = br + rd.block(x - 1, y - 1, z).light() + rd.block(x, y - 1, z + 1).light()
                                       + rd.block(x - 1, y - 1, z + 1).light();
                            } else {
                                col[0] = col[1] = col[2] = col[3] = br * 4;
                            }
                            break;
                        case 4:
                            if (NiceGrass && bl.id() == base_blocks().grass
                                && rd.block(x, y - 1, z + 1).id() == base_blocks().grass)
                                tex = Textures::getTextureIndex(bl.id(), 0);
                            br = rd.block(x, y, z + 1).light();
                            if (SmoothLighting) {
                                col[0] = br + rd.block(x - 1, y, z + 1).light() + rd.block(x, y + 1, z + 1).light()
                                       + rd.block(x - 1, y + 1, z + 1).light();
                                col[1] = br + rd.block(x - 1, y, z + 1).light() + rd.block(x, y - 1, z + 1).light()
                                       + rd.block(x - 1, y - 1, z + 1).light();
                                col[2] = br + rd.block(x + 1, y, z + 1).light() + rd.block(x, y - 1, z + 1).light()
                                       + rd.block(x + 1, y - 1, z + 1).light();
                                col[3] = br + rd.block(x + 1, y, z + 1).light() + rd.block(x, y + 1, z + 1).light()
                                       + rd.block(x + 1, y + 1, z + 1).light();
                            } else {
                                col[0] = col[1] = col[2] = col[3] = br * 4;
                            }
                            break;
                        case 5:
                            if (NiceGrass && bl.id() == base_blocks().grass
                                && rd.block(x, y - 1, z - 1).id() == base_blocks().grass)
                                tex = Textures::getTextureIndex(bl.id(), 0);
                            br = rd.block(x, y, z - 1).light();
                            if (SmoothLighting) {
                                col[0] = br + rd.block(x - 1, y, z - 1).light() + rd.block(x, y - 1, z - 1).light()
                                       + rd.block(x - 1, y - 1, z - 1).light();
                                col[1] = br + rd.block(x - 1, y, z - 1).light() + rd.block(x, y + 1, z - 1).light()
                                       + rd.block(x - 1, y + 1, z - 1).light();
                                col[2] = br + rd.block(x + 1, y, z - 1).light() + rd.block(x, y + 1, z - 1).light()
                                       + rd.block(x + 1, y + 1, z - 1).light();
                                col[3] = br + rd.block(x + 1, y, z - 1).light() + rd.block(x, y - 1, z - 1).light()
                                       + rd.block(x + 1, y - 1, z - 1).light();
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

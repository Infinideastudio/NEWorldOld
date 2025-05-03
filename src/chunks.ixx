module;

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <span>
#include <sstream>
#include <string_view>
#include <tuple>
#include <vector>
#include <leveldb/db.h>

export module chunks;
import types;
import vec3;
export import blocks;
import globals;
import frustum_tests;
import height_maps;
import rendering;
import hitboxes;

namespace chunks {

export constexpr auto SKY_LIGHT = blocks::Light(15, 0);
export constexpr auto NO_LIGHT = blocks::Light(0, 0);

export class Chunk {
public:
    // There are no lifetime requirements on `world_name` and `height_map`
    // since they are only used in the constructor.
    Chunk(Vec3i coord, leveldb::DB* db, HeightMap& height_map):
        _coord(coord) {
        if (!load_from_file(db))
            _generate(height_map);
        if (!_empty)
            _updated = true;
        loaded_chunks++;
    }
    Chunk(Chunk const&) = delete;
    Chunk(Chunk&&) = delete;
    auto operator=(Chunk const&) -> Chunk& = delete;
    auto operator=(Chunk&&) -> Chunk& = delete;
    ~Chunk() {
        destroy_meshes();
        loaded_chunks--;
        unloaded_chunks++;
    }

    auto coord() const -> Vec3i {
        return _coord;
    }

    // Hint of content
    auto empty() const -> bool {
        return _empty;
    }

    // Render is dirty
    auto updated() const -> bool {
        return _updated;
    }

    // Disk save is dirty
    auto modified() const -> bool {
        return _modified;
    }

    // Meshes are available
    auto meshed() const -> bool {
        return _meshed;
    }

    auto block(Vec3u bcoord) const -> blocks::BlockData {
        assert(bcoord.x < 16 && bcoord.y < 16 && bcoord.z < 16);
        if (_empty) {
            auto light = _coord.y < 0 ? NO_LIGHT : SKY_LIGHT;
            return blocks::BlockData{.id = base_blocks().air, .light = light};
        }
        return _data[((bcoord.x * 16) + bcoord.y) * 16 + bcoord.z];
    }

    auto block_ref(Vec3u bcoord) -> blocks::BlockData& {
        assert(bcoord.x < 16 && bcoord.y < 16 && bcoord.z < 16);
        if (_empty) {
            auto light = _coord.y < 0 ? NO_LIGHT : SKY_LIGHT;
            std::ranges::fill(_data, blocks::BlockData{.id = base_blocks().air, .light = light});
            _empty = false;
        }
        _updated = true;
        _modified = true;
        return _data[((bcoord.x * 16) + bcoord.y) * 16 + bcoord.z];
    }

    auto load_from_file(leveldb::DB* db) -> bool {
        bool exists = false;
#ifndef NEWORLD_DEBUG_NO_FILEIO
        auto key_slice = leveldb::Slice(reinterpret_cast<char*>(&_coord), sizeof(_coord));
        auto value_slice = std::string();
        auto res = db->Get(leveldb::ReadOptions(), key_slice, &value_slice);
        if (res.ok() && value_slice.size() == 4096 * sizeof(blocks::BlockData)) {
            std::memcpy(reinterpret_cast<char*>(_data.data()), value_slice.data(), 4096 * sizeof(blocks::BlockData));
            exists = true;
            _empty = _modified = false;
        }
#endif
        return exists;
    }

    auto save_to_file(leveldb::DB* db) -> bool {
        bool success = true;
#ifndef NEWORLD_DEBUG_NO_FILEIO
        if (_modified) {
            auto key_slice = leveldb::Slice(reinterpret_cast<char*>(&_coord), sizeof(_coord));
            auto value_slice = leveldb::Slice(reinterpret_cast<char*>(_data.data()), 4096 * sizeof(blocks::BlockData));
            auto res = db->Put(leveldb::WriteOptions(), key_slice, value_slice);
            success = res.ok();
            _modified = false;
        }
#endif
        return success;
    }

    void build_meshes(std::array<Chunk const*, 27> const& neighbors);

    void destroy_meshes() {
        _meshes.clear();
        _meshed = false;
        _updated = true;
    }

    void mark_neighbor_updated() {
        _updated = true;
    }

    auto load_anim() const -> float {
        return _load_anim;
    }

    void update_load_anim() {
        if (_load_anim <= 0.3f)
            _load_anim = 0.0f;
        else
            _load_anim *= 0.6f;
    }

    auto mesh(size_t index) const -> Renderer::VertexBuffer const& {
        assert(index < _meshes.size());
        return _meshes[index];
    }

    auto base_aabb() const -> Hitbox::AABB {
        auto ret = Hitbox::AABB();
        ret.xmin = _coord.x * 16 - 0.5;
        ret.xmax = _coord.x * 16 + 16 - 0.5;
        ret.ymin = _coord.y * 16 - 0.5;
        ret.ymax = _coord.y * 16 + 16 - 0.5;
        ret.zmin = _coord.z * 16 - 0.5;
        ret.zmax = _coord.z * 16 + 16 - 0.5;
        return ret;
    }

    auto relative_aabb(Vec3d const& orig) const -> FrustumTest::AABBf {
        auto ret = FrustumTest::AABBf();
        ret.xmin = static_cast<float>(_coord.x * 16 - 0.5 - orig.x);
        ret.xmax = static_cast<float>(_coord.x * 16 + 16 - 0.5 - orig.x);
        ret.ymin = static_cast<float>(_coord.y * 16 - 0.5 - _load_anim - orig.y);
        ret.ymax = static_cast<float>(_coord.y * 16 + 16 - 0.5 - _load_anim - orig.y);
        ret.zmin = static_cast<float>(_coord.z * 16 - 0.5 - orig.z);
        ret.zmax = static_cast<float>(_coord.z * 16 + 16 - 0.5 - orig.z);
        return ret;
    }

    auto visible(Vec3d const& orig, FrustumTest const& frus) const -> bool {
        return frus.test(relative_aabb(orig));
    }

private:
    Vec3i _coord;
    std::array<blocks::BlockData, 4096> _data = {};
    std::vector<Renderer::VertexBuffer> _meshes = {};

    bool _empty = true;
    bool _updated = false;
    bool _modified = false;
    bool _meshed = false;
    float _load_anim = 0.0f;

    auto _heights(HeightMap& height_map) -> std::tuple<std::array<std::array<int, 16>, 16>, int, int> {
        auto heights = std::array<std::array<int, 16>, 16>{};
        auto lo = std::numeric_limits<int>::max(), hi = height_map.WATER_LEVEL;
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                auto h = height_map.get(Vec3i(_coord.x * 16 + x, 0, _coord.z * 16 + z));
                lo = std::min(lo, h);
                hi = std::max(hi, h);
                heights[x][z] = h;
            }
        }
        auto low = (lo - 32) / 16, high = (hi + 16) / 16;
        return {heights, low, high};
    }

    void _generate_terrain(HeightMap& height_map) {
        auto [heights, low, high] = _heights(height_map);

        // Skip generation
        if (_coord.y < 0 || (_coord.y > high && _coord.y * 16 > height_map.WATER_LEVEL)) {
            return;
        }
        if (_coord.y < low) {
            std::ranges::fill(_data, blocks::BlockData{.id = base_blocks().rock, .light = NO_LIGHT});
            if (_coord.y == 0)
                for (int x = 0; x < 16; x++)
                    for (int z = 0; z < 16; z++)
                        _data[x * 256 + z].id = base_blocks().bedrock;
            _empty = false;
            return;
        }

        // Normal generation
        std::ranges::fill(_data, blocks::BlockData{.id = base_blocks().air, .light = NO_LIGHT});
        int sh = height_map.WATER_LEVEL + 2 - (_coord.y * 16);
        int wh = height_map.WATER_LEVEL - (_coord.y * 16);
        for (int x = 0; x < 16; x++) {
            for (int z = 0; z < 16; z++) {
                int base = (x << 8) + z;
                int h = heights[x][z] - (_coord.y * 16);
                if (h >= 0 || wh >= 0)
                    _empty = false;
                if (h > sh && h > wh + 1) {
                    // Grass layer
                    if (h >= 0 && h < 16)
                        _data[(h * 16) + base].id = base_blocks().grass;
                    // Dirt layer
                    for (int y = std::min(std::max(0, h - 5), 16); y < std::min(std::max(0, h), 16); y++)
                        _data[(y * 16) + base].id = base_blocks().dirt;
                } else {
                    // Sand layer
                    for (int y = std::min(std::max(0, h - 5), 16); y < std::min(std::max(0, h + 1), 16); y++)
                        _data[(y * 16) + base].id = base_blocks().sand;
                    // Water layer
                    int minh = std::min(std::max(0, h + 1), 16);
                    int maxh = std::min(std::max(0, wh + 1), 16);
                    int sky =
                        std::max(0, SKY_LIGHT.sky() - (height_map.WATER_LEVEL - (maxh - 1 + (_coord.y * 16))) * 1);
                    for (int y = maxh - 1; y >= minh; --y) {
                        sky = std::max(0, sky - 1);
                        _data[(y * 16) + base].id = base_blocks().water;
                        _data[(y * 16) + base].light = blocks::Light(sky, SKY_LIGHT.block());
                    }
                }
                // Rock layer
                for (int y = 0; y < std::min(std::max(0, h - 5), 16); y++)
                    _data[(y * 16) + base].id = base_blocks().rock;
                // Air layer
                for (int y = std::min(std::max(0, std::max(h + 1, wh + 1)), 16); y < 16; y++) {
                    _data[(y * 16) + base].id = base_blocks().air;
                    _data[(y * 16) + base].light = SKY_LIGHT;
                }
                // Bedrock layer (overwrite)
                if (_coord.y == 0)
                    _data[base].id = base_blocks().bedrock;
            }
        }
    }

    void _generate(HeightMap& height_map) {
        _generate_terrain(height_map);
    }

    auto _file_path(std::string_view world_name) const -> std::string {
        std::stringstream ss;
        ss << "worlds/" << world_name << "/chunks/chunk_" << _coord.x << "_" << _coord.y << "_" << _coord.z
           << ".neworldchunk";
        return ss.str();
    }
};

export auto const EMPTY_CHUNK = reinterpret_cast<chunks::Chunk*>(-1);
}

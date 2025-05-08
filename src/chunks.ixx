module;

#include "kls/temp/STL.h"
#undef assert

export module chunks;
import std;
import debug;
import types;
import math;
export import blocks;
import height_maps;
import render;

namespace chunks {

export class Chunk {
public:
    static constexpr auto SIZE_LOG = int32_t{4};
    static constexpr auto SIZE = int32_t{1} << SIZE_LOG;
    static constexpr auto SIZE_DATA = SIZE * SIZE * SIZE * sizeof(blocks::BlockData);

    Chunk(Vec3i coord):
        _coord(coord) {}

    // There are no lifetime requirements on `height_map`
    auto init_generate(HeightMap& height_map) {
        _generate(height_map);
    }

    auto post_init() {
        if (!_empty)
            _updated = true;
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
        assert(bcoord.x() < SIZE && bcoord.y() < SIZE && bcoord.z() < SIZE, "block coordinates out of bounds");
        if (_empty) {
            auto light = _coord.y() < 0 ? blocks::NO_LIGHT : blocks::SKY_LIGHT;
            return blocks::BlockData{.id = base_blocks().air, .light = light};
        }
        return (*_data)[((bcoord.x() * SIZE) + bcoord.y()) * SIZE + bcoord.z()];
    }

    auto block_ref(Vec3u bcoord) -> blocks::BlockData& {
        assert(bcoord.x() < SIZE && bcoord.y() < SIZE && bcoord.z() < SIZE, "block coordinates out of bounds");
        if (_empty) {
            _ensure_data();
            auto light = _coord.y() < 0 ? blocks::NO_LIGHT : blocks::SKY_LIGHT;
            std::ranges::fill(*_data, blocks::BlockData{.id = base_blocks().air, .light = light});
            _empty = false;
        }
        _updated = true;
        _modified = true;
        return (*_data)[((bcoord.x() * SIZE) + bcoord.y()) * SIZE + bcoord.z()];
    }

    auto package_to() -> kls::temp::vector<char> {
        // TODO: compression, data versioning
        auto result = kls::temp::vector<char>{};
        result.resize(SIZE_DATA);
        std::memcpy(result.data(), _data->data(), SIZE_DATA);
        return result;
    }

    auto clear_modified() {
        // TODO: maybe track this externally
        _modified = false;
    }

    auto unpackage_from(kls::temp::vector<char> data) -> bool {
        // TODO: compression, data versioning
        if (data.size() != SIZE_DATA)
            return false;
        _ensure_data();
        std::memcpy(_data->data(), data.data(), SIZE_DATA);
        _empty = _modified = false;
        return true;
    }

    void build_meshes(std::array<Chunk const*, 3 * 3 * 3> neighbors);

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

    auto mesh(size_t index) const -> std::pair<render::VertexArray, render::Buffer> const& {
        assert(index < _meshes.size(), "mesh index out of bounds");
        return _meshes[index];
    }

    auto aabb() const -> AABB3d {
        return {Vec3d(_coord * SIZE), Vec3d(_coord * SIZE + SIZE)};
    }

    auto visible(Vec3d orig, Frustumf const& frus) const -> bool {
        return frus.test(static_cast<AABB3f>(aabb() - orig) - Vec3f(0.0f, _load_anim, 0.0f));
    }

private:
    Vec3i _coord;
    std::unique_ptr<std::array<blocks::BlockData, SIZE * SIZE * SIZE>> _data{};
    std::array<std::pair<render::VertexArray, render::Buffer>, 2> _meshes{};

    bool _empty = true;
    bool _meshed = false;
    bool _updated = false;
    bool _modified = false;
    float _load_anim = 0.0f;

    auto _heights(HeightMap& height_map) -> std::tuple<std::array<std::array<int, SIZE>, SIZE>, int, int> {
        auto heights = std::array<std::array<int, SIZE>, SIZE>{};
        auto lo = std::numeric_limits<int>::max(), hi = height_map.WATER_LEVEL;
        for (int x = 0; x < SIZE; x++) {
            for (int z = 0; z < SIZE; z++) {
                auto h = height_map.get(Vec3i(_coord.x() * SIZE + x, 0, _coord.z() * SIZE + z));
                lo = std::min(lo, h);
                hi = std::max(hi, h);
                heights[x][z] = h;
            }
        }
        auto low = (lo - SIZE - 6) / SIZE, high = (hi + SIZE) / SIZE;
        return {heights, low, high};
    }

    void _generate_terrain(HeightMap& height_map) {
        auto [heights, low, high] = _heights(height_map);

        // Skip generation
        if (_coord.y() < 0 || (_coord.y() > high && _coord.y() * SIZE > height_map.WATER_LEVEL)) {
            return;
        }
        if (_coord.y() < low) {
            _ensure_data();
            std::ranges::fill(*_data, blocks::BlockData{.id = base_blocks().rock, .light = blocks::NO_LIGHT});
            if (_coord.y() == 0)
                for (int x = 0; x < SIZE; x++)
                    for (int z = 0; z < SIZE; z++)
                        (*_data)[x * SIZE * SIZE + z].id = base_blocks().bedrock;
            _empty = false;
            return;
        }

        // Normal generation
        _ensure_data();
        std::ranges::fill(*_data, blocks::BlockData{.id = base_blocks().air, .light = blocks::NO_LIGHT});
        int sh = height_map.WATER_LEVEL + 2 - (_coord.y() * SIZE);
        int wh = height_map.WATER_LEVEL - (_coord.y() * SIZE);
        for (int x = 0; x < SIZE; x++) {
            for (int z = 0; z < SIZE; z++) {
                int base = x * SIZE * SIZE + z;
                int h = heights[x][z] - (_coord.y() * SIZE);
                if (h >= 0 || wh >= 0)
                    _empty = false;
                if (h > sh && h > wh + 1) {
                    // Grass layer
                    if (h >= 0 && h < SIZE)
                        (*_data)[(h * SIZE) + base].id = base_blocks().grass;
                    // Dirt layer
                    for (int y = std::min(std::max(0, h - 5), SIZE); y < std::min(std::max(0, h), SIZE); y++)
                        (*_data)[(y * SIZE) + base].id = base_blocks().dirt;
                } else {
                    // Sand layer
                    for (int y = std::min(std::max(0, h - 5), SIZE); y < std::min(std::max(0, h + 1), SIZE); y++)
                        (*_data)[(y * SIZE) + base].id = base_blocks().sand;
                    // Water layer
                    int minh = std::min(std::max(0, h + 1), SIZE);
                    int maxh = std::min(std::max(0, wh + 1), SIZE);
                    int sky = std::max(
                        0,
                        blocks::SKY_LIGHT.sky() - (height_map.WATER_LEVEL - (maxh - 1 + (_coord.y() * SIZE))) * 1
                    );
                    for (int y = maxh - 1; y >= minh; --y) {
                        sky = std::max(0, sky - 1);
                        (*_data)[(y * SIZE) + base].id = base_blocks().water;
                        (*_data)[(y * SIZE) + base].light = blocks::Light(sky, blocks::SKY_LIGHT.block());
                    }
                }
                // Rock layer
                for (int y = 0; y < std::min(std::max(0, h - 5), SIZE); y++)
                    (*_data)[(y * SIZE) + base].id = base_blocks().rock;
                // Air layer
                for (int y = std::min(std::max(0, std::max(h + 1, wh + 1)), SIZE); y < SIZE; y++) {
                    (*_data)[(y * SIZE) + base].id = base_blocks().air;
                    (*_data)[(y * SIZE) + base].light = blocks::SKY_LIGHT;
                }
                // Bedrock layer (overwrite)
                if (_coord.y() == 0)
                    (*_data)[base].id = base_blocks().bedrock;
            }
        }
    }

    void _generate(HeightMap& height_map) {
        _generate_terrain(height_map);
    }

    void _ensure_data() {
        if (_data == nullptr)
            _data = std::make_unique<std::array<blocks::BlockData, SIZE * SIZE * SIZE>>();
    }
};
}

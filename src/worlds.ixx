module;

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <filesystem>
#include <format>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <leveldb/db.h>

export module worlds;
export import blocks;
export import chunks;
import chunk_pointer_arrays;
import frustum_tests;
import globals;
import types;
import height_maps;
import hitboxes;
import vec3;
import terrain_generation;
import rendering;

// The 64-bit chunk ID is composed of 28-bit X, 8-bit Y and 28-bit Z coordinates.
class ChunkId {
public:
    static constexpr auto MIN_X = -(int32_t{1} << 27);
    static constexpr auto MAX_X = (int32_t{1} << 27) - 1;
    static constexpr auto MIN_Y = -(int32_t{1} << 7);
    static constexpr auto MAX_Y = (int32_t{1} << 7) - 1;
    static constexpr auto MIN_Z = -(int32_t{1} << 27);
    static constexpr auto MAX_Z = (int32_t{1} << 27) - 1;

    constexpr ChunkId() = default;
    constexpr ChunkId(Vec3i coord):
        _data(pack(coord)) {}

    auto get() const -> Vec3i {
        auto x = static_cast<int32_t>(_data >> 36);
        auto y = static_cast<int32_t>(_data & 0xFF);
        auto z = static_cast<int32_t>((_data >> 8) & 0x0FFFFFFF);
        return {x, y, z};
    }

    auto operator<=>(ChunkId const&) const = default;

private:
    uint64_t _data = 0;

    static constexpr auto pack(Vec3i coord) -> uint64_t {
        auto res = uint64_t{0};
        res |= static_cast<uint64_t>(coord.x) << 36;
        res |= static_cast<uint64_t>(coord.y) & 0xFF;
        res |= (static_cast<uint64_t>(coord.z) & 0x0FFFFFFF) << 8;
        return res;
    }

    friend struct std::hash<ChunkId>;
};

template <>
struct std::hash<ChunkId> {
    auto operator()(ChunkId x) const noexcept -> size_t {
        return std::hash<uint64_t>()(x._data);
    }
};

namespace worlds {

constexpr size_t MAX_CHUNK_LOADS = 64;
constexpr size_t MAX_CHUNK_UNLOADS = 64;
constexpr size_t MAX_CHUNK_MESHINGS = 16;
constexpr size_t MAX_BLOCK_UPDATES = 65536;

export auto chunk_coord(Vec3i coord) -> Vec3i {
    // C++20 guarantees arithmetic right shift on signed integers
    // See: https://en.cppreference.com/w/cpp/language/operator_arithmetic#Built-in_bitwise_shift_operators
    return {coord.x >> 4, coord.y >> 4, coord.z >> 4};
}

export auto block_coord(Vec3i coord) -> Vec3u {
    // Signed to unsigned conversion implements modulo operation
    // See: https://en.cppreference.com/w/c/language/conversion#Integer_conversions
    auto ucoord = Vec3u(coord);
    return {ucoord.x & 0xF, ucoord.y & 0xF, ucoord.z & 0xF};
}

export class World {
public:
    explicit World(std::string name):
        _name(std::move(name)),
        _chunk_pointer_array((RenderDistance + 2) * 2),
        _height_map((RenderDistance + 2) * 2 * 16) {

        // Create world directory
        std::filesystem::create_directories(std::filesystem::path("worlds") / _name);

        // Open LevelDB database
        auto db_path = (std::filesystem::path("worlds") / _name / "chunks.db").string();
        leveldb::DB* db = nullptr;
        auto opts = leveldb::Options();
        opts.create_if_missing = true;
        auto res = leveldb::DB::Open(opts, db_path, &db);
        assert(res.ok());
        _db.reset(db);

        // Initialize terrain generation
        WorldGen::noiseInit(3404);

        // Temporary: reset counters
        loaded_chunks = 0;
        meshed_chunks = 0;
        updated_chunks = 0;
        unloaded_chunks = 0;
        updated_blocks = 0;
    }

    auto name() const -> std::string const& {
        return _name;
    }

    auto chunks() const -> std::unordered_map<ChunkId, std::unique_ptr<chunks::Chunk>> const& {
        return _chunks;
    }

    auto block_update_queue() const -> std::deque<Vec3i> const& {
        return _block_update_queue;
    }

    static auto chunk_coord_out_of_world(Vec3i ccoord) -> bool {
        return ccoord.x < ChunkId::MIN_X || ccoord.x > ChunkId::MAX_X || ccoord.y < ChunkId::MIN_Y
            || ccoord.y > ChunkId::MAX_Y || ccoord.z < ChunkId::MIN_Z || ccoord.z > ChunkId::MAX_Z;
    }

    static auto chunk_coord_out_of_range(Vec3i ccoord, Vec3i center, int dist) -> bool {
        return ccoord.x < center.x - dist || ccoord.x > center.x + dist - 1 || ccoord.y < center.y - dist
            || ccoord.y > center.y + dist - 1 || ccoord.z < center.z - dist || ccoord.z > center.z + dist - 1;
    }

    // Sets the origin of the chunk pointer array and height map
    void set_center(Vec3i ccenter) {
        _chunk_pointer_array.set_center(ccenter - Vec3i(RenderDistance + 2));
        _height_map.set_center((ccenter - Vec3i(RenderDistance + 2)) * 16);
    }

    // Saves all chunks to files
    void save_to_files() {
        for (auto const& [id, c]: _chunks)
            c->save_to_file(_db.get());
    }

    // 获取区块指针
    // 可能为 nullptr，可能为 EMPTY_CHUNK
    auto chunk(Vec3i ccoord) -> chunks::Chunk* {
        if (chunk_coord_out_of_world(ccoord))
            return nullptr;
        auto key = ChunkId(ccoord);
        if (_chunk_pointer_cache_key == key && _chunk_pointer_cache_value) {
            return _chunk_pointer_cache_value;
        }
        if (auto res = _chunk_pointer_array.get(ccoord)) {
            _chunk_pointer_cache_key = key;
            _chunk_pointer_cache_value = res;
            return res;
        }
        auto it = _chunks.find(key);
        if (it != _chunks.end()) {
            auto res = it->second.get();
            _chunk_pointer_cache_key = key;
            _chunk_pointer_cache_value = res;
            _chunk_pointer_array.set(ccoord, res);
            return res;
        }
        return nullptr;
    }

    // 获取方块和亮度
    auto block(Vec3i coord) -> std::optional<blocks::BlockData> {
        auto ccoord = chunk_coord(coord);
        auto bcoord = block_coord(coord);
        auto cptr = chunk(ccoord);
        if (!cptr)
            return {};
        if (cptr == chunks::EMPTY_CHUNK) {
            auto light = ccoord.y < 0 ? chunks::NO_LIGHT : chunks::SKY_LIGHT;
            return blocks::BlockData{.id = base_blocks().air, .light = light};
        }
        return cptr->block(bcoord);
    }

    // 带有默认值的获取方块和亮度
    auto block_or_air(Vec3i coord) -> blocks::BlockData {
        return block(coord).value_or(blocks::BlockData{.id = base_blocks().air, .light = chunks::NO_LIGHT});
    }

    // 返回与 box 相交的所有方块 AABB
    auto hitboxes(Hitbox::AABB const& box) -> std::vector<Hitbox::AABB> {
        auto res = std::vector<Hitbox::AABB>();
        for (int a = int(box.xmin + 0.5) - 1; a <= int(box.xmax + 0.5) + 1; a++) {
            for (int b = int(box.ymin + 0.5) - 1; b <= int(box.ymax + 0.5) + 1; b++) {
                for (int c = int(box.zmin + 0.5) - 1; c <= int(box.zmax + 0.5) + 1; c++) {
                    if (block_info(block_or_air(Vec3i(a, b, c)).id).solid) {
                        auto blockbox = Hitbox::AABB();
                        blockbox.xmin = a - 0.5;
                        blockbox.xmax = a + 0.5;
                        blockbox.ymin = b - 0.5;
                        blockbox.ymax = b + 0.5;
                        blockbox.zmin = c - 0.5;
                        blockbox.zmax = c + 0.5;
                        if (Hitbox::Hit(box, blockbox))
                            res.push_back(blockbox);
                    }
                }
            }
        }
        return res;
    }

    // 返回 box 是否和水方块或岩浆方块相交
    auto in_water(Hitbox::AABB const& box) -> bool {
        for (int a = int(box.xmin + 0.5) - 1; a <= int(box.xmax + 0.5) + 1; a++) {
            for (int b = int(box.ymin + 0.5) - 1; b <= int(box.ymax + 0.5) + 1; b++) {
                for (int c = int(box.zmin + 0.5) - 1; c <= int(box.zmax + 0.5) + 1; c++) {
                    auto id = block_or_air(Vec3i(a, b, c)).id;
                    if (id == base_blocks().water || id == base_blocks().lava) {
                        auto blockbox = Hitbox::AABB();
                        blockbox.xmin = a - 0.5;
                        blockbox.xmax = a + 0.5;
                        blockbox.ymin = b - 0.5;
                        blockbox.ymax = b + 0.5;
                        blockbox.zmin = c - 0.5;
                        blockbox.zmax = c + 0.5;
                        if (Hitbox::Hit(box, blockbox))
                            return true;
                    }
                }
            }
        }
        return false;
    }

    // 设置方块
    void put_block(Vec3i coord, blocks::Id value, bool update = true) {
        auto ccoord = chunk_coord(coord);
        auto bcoord = block_coord(coord);
        auto cptr = chunk(ccoord);
        if (!cptr)
            return;
        if (cptr == chunks::EMPTY_CHUNK)
            cptr = _load_chunk(ccoord);
        cptr->block_ref(bcoord).id = value;
        if (update)
            update_block(coord, true);
    }

    // Builds a tree structure
    void build_tree(Vec3i coord) {
        int th = int(rnd() * 3) + 4;
        // Tree trunk
        put_block(coord + Vec3i(0, -1, 0), base_blocks().dirt);
        for (int yt = 0; yt != th; yt++)
            put_block(coord + Vec3i(0, yt, 0), base_blocks().wood);
        // Tree leaves
        for (int xt = 0; xt != 5; xt++) {
            for (int zt = 0; zt != 5; zt++) {
                for (int yt = 0; yt != 2; yt++) {
                    if (block_or_air(coord + Vec3i(xt - 2, th - 3 + yt, zt - 2)).id == base_blocks().air)
                        put_block(coord + Vec3i(xt - 2, th - 3 + yt, zt - 2), base_blocks().leaf);
                }
            }
        }
        for (int xt = 0; xt != 3; xt++) {
            for (int zt = 0; zt != 3; zt++) {
                for (int yt = 0; yt != 2; yt++) {
                    if (block_or_air(coord + Vec3i(xt - 1, th - 1 + yt, zt - 1)).id == base_blocks().air
                        && std::abs(xt - 1) != std::abs(zt - 1))
                        put_block(coord + Vec3i(xt - 1, th - 1 + yt, zt - 1), base_blocks().leaf);
                }
            }
        }
        put_block(coord + Vec3i(0, th, 0), base_blocks().leaf);
    }

    // Destroy blocks within a radius of r
    void explode(Vec3i center, int r) {
        double maxdistsqr = r * r;
        for (int fx = center.x - r - 1; fx < center.x + r + 1; fx++) {
            for (int fy = center.y - r - 1; fy < center.y + r + 1; fy++) {
                for (int fz = center.z - r - 1; fz < center.z + r + 1; fz++) {
                    auto coord = Vec3i(fx, fy, fz);
                    int distsqr = (coord - center).length_sqr();
                    if (distsqr <= maxdistsqr * 0.75
                        || distsqr <= maxdistsqr && rnd() > (distsqr - maxdistsqr * 0.6) / (maxdistsqr * 0.4)) {
                        auto id = block_or_air(coord).id;
                        if (!block_info(id).solid)
                            continue;
                        put_block(coord, base_blocks().air);
                    }
                }
            }
        }
        // Throw at most 1000 particles from the destroyed blocks
        /*
        std::ranges::shuffle(blocks.begin(), blocks.end(), std::mt19937());
        for (size_t i = 0; i < blocks.size() && i < 1000; i++) {
            auto const& [coord, e] = blocks[i];
            Particles::throwParticle(
                e,
                float(coord.x + rnd() - 0.5f),
                float(coord.y + rnd() - 0.2f),
                float(coord.z + rnd() - 0.5f),
                float(rnd() * 2.0f - 1.0f),
                float(rnd() * 2.0f - 1.0f),
                float(rnd() * 2.0f - 1.0f),
                float(rnd() * 0.02 + 0.03),
                int(rnd() * 60) + 30
            );
        }
        */
    }

    // Triggers block update
    void update_block(Vec3i coord, bool initial = true) {
        auto ccoord = chunk_coord(coord);
        auto bcoord = block_coord(coord);

        auto cptr = chunk(ccoord);
        if (!cptr)
            return;
        if (cptr == chunks::EMPTY_CHUNK)
            cptr = _load_chunk(ccoord);

        bool updated = initial;
        auto curr = cptr->block(bcoord);

        // Explosive blocks.
        if (curr.id == base_blocks().tnt) {
            explode(coord, 8);
            return;
        }

        auto neighbors = std::array{
            block(coord + Vec3i(+1, 0, 0)),
            block(coord + Vec3i(-1, 0, 0)),
            block(coord + Vec3i(0, +1, 0)),
            block(coord + Vec3i(0, -1, 0)),
            block(coord + Vec3i(0, 0, +1)),
            block(coord + Vec3i(0, 0, -1)),
        };

        // Lighting computation.
        auto sky_light = chunks::NO_LIGHT.sky();
        auto block_light = chunks::NO_LIGHT.block();
        for (auto const& neighbor: neighbors)
            if (neighbor) {
                sky_light = std::max(sky_light, neighbor->light.sky());
                block_light = std::max(block_light, neighbor->light.block());
            }

        // If the top neighbor has maximum sky light, it and this block are both considered skylit.
        // This makes lighting computation much more efficient than the old implementation.
        // Again, blocks below (y = 0) never become skylit to prevent unbounded propagation.
        auto skylit = coord.y >= 0 && (neighbors[2] && neighbors[2]->light.sky() == chunks::SKY_LIGHT.sky());

        // Integral promption avoids underflowing when subtracting from `std::uint8_t` light levels.
        // See: https://en.cppreference.com/w/cpp/language/implicit_conversion#Integral_promotion
        if (curr.id == base_blocks().air) {
            sky_light = skylit ? chunks::SKY_LIGHT.sky() : std::max(0, sky_light - 1);
            block_light = std::max(0, block_light - 1);
        } else if (!block_info(curr.id).solid) {
            sky_light = std::max(0, sky_light - 1);
            block_light = std::max(0, block_light - 1);
        } else {
            sky_light = 0;
            block_light = 0;
        }

        // Block light sources.
        if (curr.id == base_blocks().glowstone || curr.id == base_blocks().lava)
            block_light = blocks::Light::BLOCK_MAX_VALUE;

        cptr->block_ref(bcoord).light = blocks::Light(sky_light, block_light);

        if (curr != cptr->block(bcoord))
            updated = true;

        if (updated) {
            _block_update_queue.emplace_back(coord + Vec3i(+1, 0, 0));
            _block_update_queue.emplace_back(coord + Vec3i(-1, 0, 0));
            _block_update_queue.emplace_back(coord + Vec3i(0, +1, 0));
            _block_update_queue.emplace_back(coord + Vec3i(0, -1, 0));
            _block_update_queue.emplace_back(coord + Vec3i(0, 0, +1));
            _block_update_queue.emplace_back(coord + Vec3i(0, 0, -1));

            if (bcoord.x == 15)
                _mark_chunk_neighbor_updated(ccoord + Vec3i(+1, 0, 0));
            if (bcoord.x == 0)
                _mark_chunk_neighbor_updated(ccoord + Vec3i(-1, 0, 0));
            if (bcoord.y == 15)
                _mark_chunk_neighbor_updated(ccoord + Vec3i(0, +1, 0));
            if (bcoord.y == 0)
                _mark_chunk_neighbor_updated(ccoord + Vec3i(0, -1, 0));
            if (bcoord.z == 15)
                _mark_chunk_neighbor_updated(ccoord + Vec3i(0, 0, +1));
            if (bcoord.z == 0)
                _mark_chunk_neighbor_updated(ccoord + Vec3i(0, 0, -1));

            updated_blocks++;
        }
    }

    // Process pending block updates in queue
    void process_block_updates() {
        for (size_t i = 0; i < MAX_BLOCK_UPDATES; i++) {
            if (_block_update_queue.empty())
                break;
            auto coord = _block_update_queue.front();
            _block_update_queue.pop_front();
            update_block(coord, false);
        }
    }

    void update_chunk_lists(Vec3i center) {
        auto ccenter = chunk_coord(center);

        using LoadElem = std::pair<int, Vec3i>;
        using UnloadElem = std::pair<int, Vec3i>;
        using MeshingElem = std::pair<int, chunks::Chunk*>;

        std::priority_queue<LoadElem, std::vector<LoadElem>, std::less<>> loads;
        std::priority_queue<UnloadElem, std::vector<UnloadElem>, std::greater<>> unloads;
        std::priority_queue<MeshingElem, std::vector<MeshingElem>, std::less<>> meshings;

        // Update loaded core center
        if (_loaded_core.radius > 0) {
            auto d = (ccenter - _loaded_core.ccenter).map([](int x) { return std::abs(x); });
            auto dist = static_cast<size_t>(std::max({d.x, d.y, d.z}));
            _loaded_core.radius -= std::min(_loaded_core.radius, dist);
        }
        _loaded_core.ccenter = ccenter;

        // Sort chunk load list by enumerating in cubical shells of increasing radii
        for (int radius = int(_loaded_core.radius) + 1; radius <= RenderDistance + 1; radius++) {
            // Enumerate cubical shell with side length (dist * 2)
            for (int cx = ccenter.x - radius; cx < ccenter.x + radius; cx++) {
                for (int cy = ccenter.y - radius; cy < ccenter.y + radius; cy++) {
                    // Skip interior of cubical shell
                    int stride = radius * 2 - 1;
                    if (cx == ccenter.x - radius || cx == ccenter.x + radius - 1)
                        stride = 1;
                    if (cy == ccenter.y - radius || cy == ccenter.y + radius - 1)
                        stride = 1;
                    // If both X and Y are interior, Z only checks two points
                    for (int cz = ccenter.z - radius; cz < ccenter.z + radius; cz += stride) {
                        auto cc = Vec3i(cx, cy, cz);
                        if (!chunk_coord_out_of_world(cc) && !chunk(cc)) {
                            auto distsqr = (cc * 16 + 8 - center).length_sqr();
                            loads.emplace(distsqr, cc);
                            if (loads.size() > MAX_CHUNK_LOADS)
                                loads.pop();
                        }
                    }
                }
            }
            // Update loaded core radius for the known part
            if (loads.empty())
                _loaded_core.radius = radius;
            // Break if already complete
            if (loads.size() == MAX_CHUNK_LOADS)
                break;
        }

        // Sort chunk unload and meshing lists simultaneously
        for (auto const& [_, c]: _chunks) {
            auto cc = c->coord();
            if (chunk_coord_out_of_range(cc, ccenter, RenderDistance + 1)) {
                auto distsqr = (cc * 16 + 8 - center).length_sqr();
                unloads.emplace(distsqr, cc);
                if (unloads.size() > MAX_CHUNK_UNLOADS)
                    unloads.pop();
            } else if (!chunk_coord_out_of_range(cc, ccenter, RenderDistance) && c->updated()) {
                auto distsqr = (cc * 16 + 8 - center).length_sqr();
                meshings.emplace(distsqr, c.get());
                if (meshings.size() > MAX_CHUNK_MESHINGS)
                    meshings.pop();
            }
        }

        // Write results back
        _chunk_load_list.clear();
        _chunk_unload_list.clear();
        _chunk_meshing_list.clear();

        while (!loads.empty()) {
            _chunk_load_list.emplace_back(loads.top());
            loads.pop();
        }
        while (!unloads.empty()) {
            _chunk_unload_list.emplace_back(unloads.top());
            unloads.pop();
        }
        while (!meshings.empty()) {
            _chunk_meshing_list.emplace_back(meshings.top());
            meshings.pop();
        }
    }

    void process_chunk_loads() {
        for (auto [_, ccoord]: _chunk_load_list) {
            _load_chunk(ccoord, true);
        }
    }

    void process_chunk_unloads() {
        for (auto [_, ccoord]: _chunk_unload_list) {
            _unload_chunk(ccoord);
        }
    }

    void process_chunk_meshings() {
        for (auto [_, c]: _chunk_meshing_list) {
            auto ccoord = c->coord();
            auto neighbors = std::array<chunks::Chunk const*, 27>{};
            auto all = true;
            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    for (int z = -1; z <= 1; z++) {
                        auto cptr = chunk(ccoord + Vec3i(x, y, z));
                        if (!cptr)
                            all = false;
                        neighbors[((x + 1) * 3 + (y + 1)) * 3 + (z + 1)] = cptr;
                    }
                }
            }
            if (all)
                c->build_meshes(neighbors);
        }
    }

    using RenderChunk = std::pair<Vec3d, std::array<Renderer::VertexBuffer const*, 2>>;

    auto list_render_chunks(Vec3d const& center, int dist, double interp, std::optional<FrustumTest> frustum)
        -> std::vector<RenderChunk>;

    void render_chunks(Vec3d const& center, std::vector<RenderChunk> const& crs, size_t index);

private:
    struct LoadedCore {
        Vec3i ccenter = Vec3i(0, 0, 0);
        size_t radius = 0;
    };

    std::string _name;
    std::unique_ptr<leveldb::DB> _db;
    std::unordered_map<ChunkId, std::unique_ptr<chunks::Chunk>> _chunks;
    std::vector<std::pair<int, chunks::Chunk*>> _chunk_meshing_list;
    std::vector<std::pair<int, Vec3i>> _chunk_load_list;
    std::vector<std::pair<int, Vec3i>> _chunk_unload_list;
    std::deque<Vec3i> _block_update_queue;

    ChunkId _chunk_pointer_cache_key = ChunkId(Vec3i(0, 0, 0));
    chunks::Chunk* _chunk_pointer_cache_value = nullptr;
    ChunkPointerArray _chunk_pointer_array;
    HeightMap _height_map;
    LoadedCore _loaded_core;

    auto _load_chunk(Vec3i ccoord, bool skip_empty = false) -> chunks::Chunk* {
        auto cid = ChunkId(ccoord);
        auto it = _chunks.find(cid);
        if (it != _chunks.end()) {
            DebugWarning(std::format("Trying to load existing chunk ({}, {}, {})", ccoord.x, ccoord.y, ccoord.z));
            return it->second.get();
        }

        // Load chunk from file if exists
        auto handle = std::make_unique<chunks::Chunk>(ccoord, _db.get(), _height_map);
        auto cptr = handle.get();

        // Optionally skip empty chunks
        if (skip_empty && cptr->empty()) {
            _chunk_pointer_array.set(ccoord, chunks::EMPTY_CHUNK);
            return chunks::EMPTY_CHUNK;
        }

        // Update caches
        _chunk_pointer_cache_key = cid;
        _chunk_pointer_cache_value = cptr;
        _chunk_pointer_array.set(ccoord, cptr);

        _chunks.emplace(cid, std::move(handle));
        return cptr;
    }

    void _unload_chunk(Vec3i ccoord) {
        auto cid = ChunkId(ccoord);
        auto node = _chunks.extract(cid);
        if (node.empty()) {
            DebugWarning(std::format("Trying to unload non-existing chunk ({}, {}, {})", ccoord.x, ccoord.y, ccoord.z));
            return;
        }

        // Save chunk to file if modified
        auto cptr = node.mapped().get();
        cptr->save_to_file(_db.get());

        // Update caches
        if (_chunk_pointer_cache_value == cptr) {
            _chunk_pointer_cache_key = ChunkId(Vec3i(0, 0, 0));
            _chunk_pointer_cache_value = nullptr;
        }
        _chunk_pointer_array.set(ccoord, nullptr);

        // Shrink loaded core
        auto d = (ccoord - _loaded_core.ccenter).map([](int x) { return std::abs(x); });
        auto dist = static_cast<size_t>(std::max({d.x, d.y, d.z}));
        _loaded_core.radius = std::min(_loaded_core.radius, dist);
    }

    void _mark_chunk_neighbor_updated(Vec3i ccoord) {
        auto cptr = chunk(ccoord);
        if (!cptr || cptr == chunks::EMPTY_CHUNK)
            return;
        cptr->mark_neighbor_updated();
    }
};
}

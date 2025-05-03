module;

#include <algorithm>
#include <array>
#include <cassert>
#include <fstream>
#include <sstream>
#include <string_view>
#include <tuple>
#include <vector>

export module chunks;
import vec3;
import types;
export import blocks;
import globals;
import frustum_tests;
import height_maps;
import rendering;
import hitboxes;

export using ChunkID = uint64_t;

export constexpr int WorldSize = 134217728;
export constexpr int WorldHeight = 128;
export constexpr auto SKY_LIGHT = BlockData::Light(15, 0);
export constexpr auto NO_LIGHT = BlockData::Light(0, 0);

export auto getChunkPos(Vec3i coord) -> Vec3i {
    // C++20 guarantees arithmetic right shift on signed integers
    // See: https://en.cppreference.com/w/cpp/language/operator_arithmetic#Built-in_bitwise_shift_operators
    return {coord.x >> 4, coord.y >> 4, coord.z >> 4};
}

export auto getBlockPos(Vec3i coord) -> Vec3u {
    // Signed to unsigned conversion implements modulo operation
    // See: https://en.cppreference.com/w/c/language/conversion#Integer_conversions
    auto ucoord = Vec3u(coord);
    return {ucoord.x & 0xF, ucoord.y & 0xF, ucoord.z & 0xF};
}

export auto getChunkID(Vec3i coord) -> ChunkID {
    if (coord.y == -128)
        coord.y = 0;
    if (coord.y <= 0)
        coord.y = std::abs(coord.y) + (1 << 7);
    if (coord.x == -134217728)
        coord.x = 0;
    if (coord.x <= 0)
        coord.x = std::abs(coord.x) + (1 << 27);
    if (coord.z == -134217728)
        coord.z = 0;
    if (coord.z <= 0)
        coord.z = std::abs(coord.z) + (1 << 27);
    return (ChunkID(coord.y) << 56) + (ChunkID(coord.x) << 28) + coord.z;
}

export auto chunkOutOfBound(Vec3i coord) -> bool {
    return coord.y < -WorldHeight || coord.y > WorldHeight - 1 || coord.x < -134217728 || coord.x > 134217727
        || coord.z < -134217728 || coord.z > 134217727;
}

export class Chunk {
public:
    // There are no lifetime requirements on `worldName` and `heightMap`
    // since they are only used in the constructor.
    Chunk(Vec3i coord, std::string_view worldName, HeightMap& heightMap):
        ccoord(coord),
        cid(getChunkID(coord)) {
        if (!loadFromFile(worldName))
            build(heightMap);
        if (!isEmpty)
            isUpdated = true;
        loadedChunks++;
    }
    Chunk(Chunk const&) = delete;
    Chunk(Chunk&&) = delete;
    auto operator=(Chunk const&) -> Chunk& = delete;
    auto operator=(Chunk&&) -> Chunk& = delete;
    ~Chunk() {
        destroyMeshes();
        loadedChunks--;
        unloadedChunks++;
    }

    auto coord() const -> Vec3i {
        return ccoord;
    }

    auto id() const -> ChunkID {
        return cid;
    }

    // Hint of content
    auto empty() const -> bool {
        return isEmpty;
    }

    // Render is dirty
    auto updated() const -> bool {
        return isUpdated;
    }

    // Disk save is dirty
    auto modified() const -> bool {
        return isModified;
    }

    // All details generated
    auto ready() const -> bool {
        return true;
    }

    // Meshes are available
    auto meshed() const -> bool {
        return isMeshed;
    }

    auto block(Vec3u bcoord) const -> BlockData {
        assert(coord.x < 16 && coord.y < 16 && coord.z < 16);
        if (isEmpty) {
            auto light = ccoord.y < 0 ? NO_LIGHT : SKY_LIGHT;
            return BlockData{.id = Blocks().air, .light = light};
        }
        return blocks[((bcoord.x * 16) + bcoord.y) * 16 + bcoord.z];
    }

    auto block_ref(Vec3u bcoord) -> BlockData& {
        assert(coord.x < 16 && coord.y < 16 && coord.z < 16);
        if (isEmpty) {
            auto light = ccoord.y < 0 ? NO_LIGHT : SKY_LIGHT;
            std::ranges::fill(blocks, BlockData{.id = Blocks().air, .light = light});
            isEmpty = false;
        }
        isUpdated = true;
        isModified = true;
        return blocks[((bcoord.x * 16) + bcoord.y) * 16 + bcoord.z];
    }

    auto loadFromFile(std::string_view worldName) -> bool {
        bool exists = false;
#ifndef NEWORLD_DEBUG_NO_FILEIO
        auto file = std::ifstream(getChunkPath(worldName), std::ios::in | std::ios::binary);
        exists = file.is_open();
        if (exists) {
            file.read(reinterpret_cast<char*>(blocks.data()), 4096 * sizeof(BlockData));
            file.read(reinterpret_cast<char*>(&isDetailGenerated), sizeof(bool));
            isEmpty = isModified = false;
        }
#endif
        return exists;
    }

    auto saveToFile(std::string_view worldName) -> bool {
        bool success = true;
#ifndef NEWORLD_DEBUG_NO_FILEIO
        if (isModified) {
            auto file = std::ofstream(getChunkPath(worldName), std::ios::out | std::ios::binary);
            success = file.is_open();
            if (success) {
                file.write(reinterpret_cast<char*>(blocks.data()), 4096 * sizeof(BlockData));
                file.write(reinterpret_cast<char*>(&isDetailGenerated), sizeof(bool));
                isModified = false;
            }
        }
#endif
        return success;
    }

    void buildMeshes(std::array<Chunk const*, 27> const& neighbors);

    void destroyMeshes() {
        meshes.clear();
        isMeshed = false;
        isUpdated = true;
    }

    void markNeighborUpdated() {
        isUpdated = true;
    }

    auto loadAnimOffset() const -> float {
        return loadAnim;
    }
    void updateLoadAnimOffset() {
        if (loadAnim <= 0.3f)
            loadAnim = 0.0f;
        else
            loadAnim *= 0.6f;
    }

    auto mesh(size_t index) const -> Renderer::VertexBuffer const& {
        assert(index < meshes.size());
        return meshes[index];
    }

    auto baseAABB() const -> Hitbox::AABB {
        auto ret = Hitbox::AABB();
        ret.xmin = ccoord.x * 16 - 0.5;
        ret.xmax = ccoord.x * 16 + 16 - 0.5;
        ret.ymin = ccoord.y * 16 - 0.5;
        ret.ymax = ccoord.y * 16 + 16 - 0.5;
        ret.zmin = ccoord.z * 16 - 0.5;
        ret.zmax = ccoord.z * 16 + 16 - 0.5;
        return ret;
    }

    auto relativeAABB(Vec3d const& orig) const -> FrustumTest::AABBf {
        auto ret = FrustumTest::AABBf();
        ret.xmin = static_cast<float>(ccoord.x * 16 - 0.5 - orig.x);
        ret.xmax = static_cast<float>(ccoord.x * 16 + 16 - 0.5 - orig.x);
        ret.ymin = static_cast<float>(ccoord.y * 16 - 0.5 - loadAnim - orig.y);
        ret.ymax = static_cast<float>(ccoord.y * 16 + 16 - 0.5 - loadAnim - orig.y);
        ret.zmin = static_cast<float>(ccoord.z * 16 - 0.5 - orig.z);
        ret.zmax = static_cast<float>(ccoord.z * 16 + 16 - 0.5 - orig.z);
        return ret;
    }

    auto visible(Vec3d const& orig, FrustumTest const& frus) const -> bool {
        return frus.test(relativeAABB(orig));
    }

private:
    Vec3i ccoord;
    ChunkID cid;
    std::array<BlockData, 4096> blocks = {};

    bool isEmpty = false;
    bool isUpdated = false;
    bool isModified = false;
    bool isDetailGenerated = false;
    bool isMeshed = false;

    std::vector<Renderer::VertexBuffer> meshes;
    float loadAnim = 0.0f;

    auto getHeights(HeightMap& heightMap, int cx, int cz) -> std::tuple<std::array<std::array<int, 16>, 16>, int, int> {
        auto heights = std::array<std::array<int, 16>, 16>{};
        int lo = std::numeric_limits<int>::max(), hi = heightMap.WaterLevel;
        for (int x = 0; x < 16; ++x) {
            for (int z = 0; z < 16; ++z) {
                int h = heightMap.getHeight(Vec3i(cx * 16 + x, 0, cz * 16 + z));
                if (h < lo)
                    lo = h;
                if (h > hi)
                    hi = h;
                heights[x][z] = h;
            }
        }
        int low = (lo - 21) / 16, high = (hi + 16) / 16;
        return {heights, low, high};
    }

    void buildTerrain(HeightMap& heightMap) {
        auto [heights, low, high] = getHeights(heightMap, ccoord.x, ccoord.z);

        // Skip generation
        if (ccoord.y < 0 || (ccoord.y > high && ccoord.y * 16 > heightMap.WaterLevel)) {
            isEmpty = true;
            return;
        }
        if (ccoord.y < low) {
            std::ranges::fill(blocks, BlockData{.id = Blocks().rock, .light = NO_LIGHT});
            if (ccoord.y == 0)
                for (int x = 0; x < 16; x++)
                    for (int z = 0; z < 16; z++)
                        blocks[x * 256 + z].id = Blocks().bedrock;
            isEmpty = false;
            return;
        }

        // Normal generation
        std::ranges::fill(blocks, BlockData{.id = Blocks().air, .light = NO_LIGHT});

        int h = 0, sh = 0, wh = 0;

        isEmpty = true;
        sh = heightMap.WaterLevel + 2 - (ccoord.y * 16);
        wh = heightMap.WaterLevel - (ccoord.y * 16);

        for (int x = 0; x < 16; ++x) {
            for (int z = 0; z < 16; ++z) {
                int base = (x << 8) + z;
                h = heights[x][z] - (ccoord.y * 16);
                if (h >= 0 || wh >= 0)
                    isEmpty = false;
                if (h > sh && h > wh + 1) {
                    // Grass layer
                    if (h >= 0 && h < 16)
                        blocks[(h * 16) + base].id = Blocks().grass;
                    // Dirt layer
                    for (int y = std::min(std::max(0, h - 5), 16); y < std::min(std::max(0, h), 16); ++y)
                        blocks[(y * 16) + base].id = Blocks().dirt;
                } else {
                    // Sand layer
                    for (int y = std::min(std::max(0, h - 5), 16); y < std::min(std::max(0, h + 1), 16); ++y)
                        blocks[(y * 16) + base].id = Blocks().sand;
                    // Water layer
                    int minh = std::min(std::max(0, h + 1), 16);
                    int maxh = std::min(std::max(0, wh + 1), 16);
                    int sky = std::max(0, SKY_LIGHT.sky() - (heightMap.WaterLevel - (maxh - 1 + (ccoord.y * 16))) * 1);
                    for (int y = maxh - 1; y >= minh; --y) {
                        sky = std::max(0, sky - 1);
                        blocks[(y * 16) + base].id = Blocks().water;
                        blocks[(y * 16) + base].light = BlockData::Light(sky, SKY_LIGHT.block());
                    }
                }
                // Rock layer
                for (int y = 0; y < std::min(std::max(0, h - 5), 16); ++y)
                    blocks[(y * 16) + base].id = Blocks().rock;
                // Air layer
                for (int y = std::min(std::max(0, std::max(h + 1, wh + 1)), 16); y < 16; ++y) {
                    blocks[(y * 16) + base].id = Blocks().air;
                    blocks[(y * 16) + base].light = SKY_LIGHT;
                }
                // Bedrock layer (overwrite)
                if (ccoord.y == 0)
                    blocks[base].id = Blocks().bedrock;
            }
        }
    }

    void buildDetail() {
        /*
        int index = 0;
        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 16; y++) {
                for (int z = 0; z < 16; z++) {
                    // Tree
                    if (blocks[index].id == Blocks().grass && rnd() < 0.005)
                        buildtree(cx * 16 + x, ccoord.y * 16 + y, cz * 16 + z);
                    index++;
                }
            }
        }
        */
    }

    void build(HeightMap& heightMap) {
        buildTerrain(heightMap);
        // if (!Empty) buildDetail();
    }

    auto getChunkPath(std::string_view worldName) const -> std::string {
        std::stringstream ss;
        ss << "worlds/" << worldName << "/chunks/chunk_" << ccoord.x << "_" << ccoord.y << "_" << ccoord.z
           << ".neworldchunk";
        return ss.str();
    }
};

export auto const EmptyChunkPtr = reinterpret_cast<Chunk*>(-1);

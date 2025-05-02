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

constexpr size_t MaxChunkLoads = 64;
constexpr size_t MaxChunkUnloads = 64;
constexpr size_t MaxChunkMeshings = 16;
constexpr size_t MaxBlockUpdates = 4096;

export class World {
public:
    std::string WorldName;
    std::unordered_map<ChunkID, std::unique_ptr<Chunk>> chunks;
    std::vector<std::pair<int, Chunk*>> chunkMeshingList;
    std::vector<std::pair<int, Vec3i>> chunkLoadList;
    std::vector<std::pair<int, Vec3i>> chunkUnloadList;
    std::deque<Vec3i> blockUpdateQueue;

    explicit World(std::string worldName):
        WorldName(std::move(worldName)),
        chunkPtrArray((RenderDistance + 2) * 2),
        heightMap((RenderDistance + 2) * 2 * 1) {

        // Create world and chunk directory
        std::filesystem::create_directories(std::filesystem::path("worlds") / WorldName / "chunks");

        // Initialize terrain generation
        WorldGen::noiseInit(3404);

        // Temporary: reset counters
        loadedChunks = 0;
        meshedChunks = 0;
        updatedChunks = 0;
        unloadedChunks = 0;
        updatedBlocks = 0;
    }

    void setCenter(Vec3i ccenter) {
        chunkPtrArray.moveTo(ccenter - Vec3i(RenderDistance + 2));
        heightMap.moveTo((ccenter - Vec3i(RenderDistance + 2)) * 16);
    }

    auto addChunk(Vec3i ccoord) -> Chunk* {
        ChunkID cid = getChunkID(ccoord);
        auto it = chunks.find(cid);
        if (it != chunks.end()) {
            DebugWarning(
                std::format("Chunk ({}, {}, {}) has been loaded, when adding chunk.", ccoord.x, ccoord.y, ccoord.z)
            );
            return it->second.get();
        }

        auto c = std::make_unique<Chunk>(ccoord, WorldName, heightMap);
        Chunk* res = c.get();
        chunks.emplace(cid, std::move(c));

        chunkPtrCacheKey = cid;
        chunkPtrCacheValue = res;
        chunkPtrArray.setChunkPtr(ccoord, res);
        return res;
    }

    void removeChunk(Vec3i ccoord, bool setEmpty = false) {
        ChunkID cid = getChunkID(ccoord);
        auto node = chunks.extract(cid);
        if (node.empty()) {
            DebugWarning(
                std::format("Chunk ({}, {}, {}) has been unloaded, when deleting chunk.", ccoord.x, ccoord.y, ccoord.z)
            );
            return;
        }

        if (chunkPtrCacheValue == node.mapped().get()) {
            chunkPtrCacheKey = 0;
            chunkPtrCacheValue = nullptr;
        }
        chunkPtrArray.setChunkPtr(ccoord, setEmpty ? EmptyChunkPtr : nullptr);

        // Shrink loaded core
        auto d = (ccoord - loadedCore.ccenter).map([](int x) { return std::abs(x); });
        auto dist = static_cast<size_t>(std::max({d.x, d.y, d.z}));
        loadedCore.radius = std::min(loadedCore.radius, dist);
    }

    auto getChunkPtr(Vec3i ccoord) -> Chunk* {
        ChunkID cid = getChunkID(ccoord);
        if (chunkPtrCacheKey == cid && chunkPtrCacheValue != nullptr) {
            return chunkPtrCacheValue;
        }
        Chunk* ret = chunkPtrArray.getChunkPtr(ccoord);
        if (ret != nullptr) {
            chunkPtrCacheKey = cid;
            chunkPtrCacheValue = ret;
            return ret;
        }
        auto it = chunks.find(cid);
        if (it != chunks.end()) {
            ret = it->second.get();
            chunkPtrCacheKey = cid;
            chunkPtrCacheValue = ret;
            chunkPtrArray.setChunkPtr(ccoord, ret);
            return ret;
        }
        return nullptr;
    }

    auto chunkLoaded(Vec3i ccoord) -> bool {
        if (chunkOutOfBound(ccoord))
            return false;
        if (getChunkPtr(ccoord) != nullptr)
            return true;
        return false;
    }

    auto getHitboxes(const Hitbox::AABB& box) -> std::vector<Hitbox::AABB> {
        // 返回与box相交的所有方块AABB
        auto res = std::vector<Hitbox::AABB>();
        for (int a = int(box.xmin + 0.5) - 1; a <= int(box.xmax + 0.5) + 1; a++) {
            for (int b = int(box.ymin + 0.5) - 1; b <= int(box.ymax + 0.5) + 1; b++) {
                for (int c = int(box.zmin + 0.5) - 1; c <= int(box.zmax + 0.5) + 1; c++) {
                    if (BlockInfo(getBlock(Vec3i(a, b, c))).isSolid()) {
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

    auto inWater(const Hitbox::AABB& box) -> bool {
        for (int a = int(box.xmin + 0.5) - 1; a <= int(box.xmax + 0.5) + 1; a++) {
            for (int b = int(box.ymin + 0.5) - 1; b <= int(box.ymax + 0.5) + 1; b++) {
                for (int c = int(box.zmin + 0.5) - 1; c <= int(box.zmax + 0.5) + 1; c++) {
                    auto id = getBlock(Vec3i(a, b, c));
                    if (id == BlockID::WATER || id == BlockID::LAVA) {
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

    // Trigger block update
    void updateBlock(Vec3i coord, bool initial = true) {
        auto ccoord = getChunkPos(coord);
        auto bcoord = getBlockPos(coord);

        if (chunkOutOfBound(ccoord))
            return;

        auto cptr = getChunkPtr(ccoord);
        if (cptr != nullptr) {
            if (cptr == EmptyChunkPtr)
                cptr = World::addChunk(ccoord);

            bool updated = initial;
            BlockID oldblock = cptr->getBlock(bcoord.x, bcoord.y, bcoord.z);
            Brightness oldbrightness = cptr->getBrightness(bcoord.x, bcoord.y, bcoord.z);

            bool skylit = true;
            if (coord.y < 0) {
                skylit = false;
            } else {
                auto up = coord;
                up.y += 1;
                while (chunkLoaded(getChunkPos(up))) {
                    auto id = getBlock(up);
                    if (BlockInfo(id).isOpaque() || id == BlockID::WATER) {
                        skylit = false;
                        break;
                    }
                    up.y += 1;
                }
            }

            if (oldblock == BlockID::TNT) {
                explode(coord, 8);
                return;
            }

            if (oldblock == BlockID::GLOWSTONE || oldblock == BlockID::LAVA) {
                cptr->setBrightness(bcoord.x, bcoord.y, bcoord.z, MaxBrightness);
            } else if (!BlockInfo(oldblock).isOpaque()) {
                auto blks = std::array{
                    getBlock(coord + Vec3i(+1, 0, 0)),
                    getBlock(coord + Vec3i(-1, 0, 0)),
                    getBlock(coord + Vec3i(0, +1, 0)),
                    getBlock(coord + Vec3i(0, -1, 0)),
                    getBlock(coord + Vec3i(0, 0, +1)),
                    getBlock(coord + Vec3i(0, 0, -1)),
                };
                auto brts = std::array{
                    getBrightness(coord + Vec3i(+1, 0, 0)),
                    getBrightness(coord + Vec3i(-1, 0, 0)),
                    getBrightness(coord + Vec3i(0, +1, 0)),
                    getBrightness(coord + Vec3i(0, -1, 0)),
                    getBrightness(coord + Vec3i(0, 0, +1)),
                    getBrightness(coord + Vec3i(0, 0, -1)),
                };
                int maxbrightness = 0;
                for (int i = 0; i < 6; i++)
                    if (brts[maxbrightness] < brts[i])
                        maxbrightness = i;
                auto br = brts[maxbrightness];

                if (blks[maxbrightness] == BlockID::WATER) {
                    if (br < MinBrightness + 2)
                        br = MinBrightness;
                    else
                        br -= 2;
                } else {
                    if (br < MinBrightness + 1)
                        br = MinBrightness;
                    else
                        br--;
                }
                if (skylit) {
                    if (br < SkyBrightness)
                        br = SkyBrightness;
                }
                if (br < MinBrightness)
                    br = MinBrightness;

                cptr->setBrightness(bcoord.x, bcoord.y, bcoord.z, br);
            } else {
                cptr->setBrightness(bcoord.x, bcoord.y, bcoord.z, 0);
            }

            if (oldblock != cptr->getBlock(bcoord.x, bcoord.y, bcoord.z))
                updated = true;
            if (oldbrightness != cptr->getBrightness(bcoord.x, bcoord.y, bcoord.z))
                updated = true;

            if (updated) {
                blockUpdateQueue.emplace_back(coord + Vec3i(+1, 0, 0));
                blockUpdateQueue.emplace_back(coord + Vec3i(-1, 0, 0));
                blockUpdateQueue.emplace_back(coord + Vec3i(0, +1, 0));
                blockUpdateQueue.emplace_back(coord + Vec3i(0, -1, 0));
                blockUpdateQueue.emplace_back(coord + Vec3i(0, 0, +1));
                blockUpdateQueue.emplace_back(coord + Vec3i(0, 0, -1));

                if (bcoord.x == 15 && ccoord.x < WorldSize - 1)
                    markChunkNeighborUpdated(ccoord + Vec3i(+1, 0, 0));
                if (bcoord.x == 0 && ccoord.x > -WorldSize)
                    markChunkNeighborUpdated(ccoord + Vec3i(-1, 0, 0));
                if (bcoord.y == 15 && ccoord.y < WorldHeight - 1)
                    markChunkNeighborUpdated(ccoord + Vec3i(0, +1, 0));
                if (bcoord.y == 0 && ccoord.y > -WorldHeight)
                    markChunkNeighborUpdated(ccoord + Vec3i(0, -1, 0));
                if (bcoord.z == 15 && ccoord.z < WorldSize - 1)
                    markChunkNeighborUpdated(ccoord + Vec3i(0, 0, +1));
                if (bcoord.z == 0 && ccoord.z > -WorldSize)
                    markChunkNeighborUpdated(ccoord + Vec3i(0, 0, -1));

                updatedBlocks++;
            }
        }
    }

    // Process pending block updates in queue
    void updateBlocks() {
        for (size_t i = 0; i < MaxBlockUpdates; i++) {
            if (blockUpdateQueue.empty())
                break;
            auto coord = blockUpdateQueue.front();
            blockUpdateQueue.pop_front();
            updateBlock(coord, false);
        }
    }

    // 获取方块
    auto getBlock(Vec3i coord, BlockID mask = BlockID::AIR) -> BlockID {
        auto ccoord = getChunkPos(coord);
        auto bcoord = getBlockPos(coord);
        if (chunkOutOfBound(ccoord))
            return BlockID::AIR;
        auto cptr = getChunkPtr(ccoord);
        if (cptr == EmptyChunkPtr)
            return BlockID::AIR;
        if (cptr != nullptr)
            return cptr->getBlock(bcoord.x, bcoord.y, bcoord.z);
        return mask;
    }

    // 获取亮度
    auto getBrightness(Vec3i coord) -> Brightness {
        auto ccoord = getChunkPos(coord);
        auto bcoord = getBlockPos(coord);
        if (chunkOutOfBound(ccoord))
            return SkyBrightness;
        auto cptr = getChunkPtr(ccoord);
        if (cptr == EmptyChunkPtr)
            return ccoord.y < 0 ? MinBrightness : SkyBrightness;
        if (cptr != nullptr)
            return cptr->getBrightness(bcoord.x, bcoord.y, bcoord.z);
        return SkyBrightness;
    }

    // 设置方块
    void setBlock(Vec3i coord, BlockID value, bool update = true) {
        auto ccoord = getChunkPos(coord);
        auto bcoord = getBlockPos(coord);
        if (!chunkOutOfBound(ccoord)) {
            auto cptr = getChunkPtr(ccoord);
            if (cptr == EmptyChunkPtr)
                cptr = addChunk(ccoord);
            if (cptr != nullptr) {
                cptr->setBlock(bcoord.x, bcoord.y, bcoord.z, value);
                if (update)
                    updateBlock(coord, true);
            }
        }
    }

    auto chunkInRange(Vec3i ccoord, Vec3i center, int dist) const -> bool {
        if (ccoord.x < center.x - dist || ccoord.x > center.x + dist - 1 || ccoord.y < center.y - dist
            || ccoord.y > center.y + dist - 1 || ccoord.z < center.z - dist || ccoord.z > center.z + dist - 1)
            return false;
        return true;
    }

    auto chunkUpdated(Vec3i coord) -> bool {
        auto i = getChunkPtr(coord);
        if (i == nullptr || i == EmptyChunkPtr)
            return false;
        return i->updated();
    }

    void markChunkNeighborUpdated(Vec3i coord) {
        auto i = getChunkPtr(coord);
        if (i == nullptr || i == EmptyChunkPtr)
            return;
        i->markNeighborUpdated();
    }

    void sortChunkUpdateLists(Vec3i center) {
        auto ccenter = getChunkPos(center);

        using LoadElem = std::pair<int, Vec3i>;
        using UnloadElem = std::pair<int, Vec3i>;
        using MeshingElem = std::pair<int, Chunk*>;

        std::priority_queue<LoadElem, std::vector<LoadElem>, std::less<>> loads;
        std::priority_queue<UnloadElem, std::vector<UnloadElem>, std::greater<>> unloads;
        std::priority_queue<MeshingElem, std::vector<MeshingElem>, std::less<>> meshings;

        // Update loaded core center
        if (loadedCore.radius > 0) {
            auto d = (ccenter - loadedCore.ccenter).map([](int x) { return std::abs(x); });
            auto dist = static_cast<size_t>(std::max({d.x, d.y, d.z}));
            loadedCore.radius -= std::min(loadedCore.radius, dist);
        }
        loadedCore.ccenter = ccenter;

        // Sort chunk load list by enumerating in cubical shells of increasing radii
        for (int radius = int(loadedCore.radius) + 1; radius <= RenderDistance + 1; radius++) {
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
                        if (chunkOutOfBound(cc))
                            continue;
                        if (getChunkPtr(cc) == nullptr) {
                            auto distsqr = (cc * 16 + 8 - center).lengthSqr();
                            loads.emplace(distsqr, cc);
                            if (loads.size() > MaxChunkLoads)
                                loads.pop();
                        }
                    }
                }
            }
            // Update loaded core radius for the known part
            if (loads.empty())
                loadedCore.radius = radius;
            // Break if already complete
            if (loads.size() == MaxChunkLoads)
                break;
        }

        // Sort chunk unload and meshing lists simultaneously
        for (auto const& [_, c]: chunks) {
            auto cc = c->coord();
            if (!chunkInRange(cc, ccenter, RenderDistance + 1)) {
                auto distsqr = (cc * 16 + 8 - center).lengthSqr();
                unloads.emplace(distsqr, cc);
                if (unloads.size() > MaxChunkUnloads)
                    unloads.pop();
            } else if (chunkInRange(cc, ccenter, RenderDistance) && c->updated()) {
                auto distsqr = (cc * 16 + 8 - center).lengthSqr();
                meshings.emplace(distsqr, c.get());
                if (meshings.size() > MaxChunkMeshings)
                    meshings.pop();
            }
        }

        // Write results back
        chunkLoadList.clear();
        chunkUnloadList.clear();
        chunkMeshingList.clear();

        while (!loads.empty()) {
            chunkLoadList.emplace_back(loads.top());
            loads.pop();
        }
        while (!unloads.empty()) {
            chunkUnloadList.emplace_back(unloads.top());
            unloads.pop();
        }
        while (!meshings.empty()) {
            chunkMeshingList.emplace_back(meshings.top());
            meshings.pop();
        }
    }

    void buildtree(Vec3i coord) {
        int th = int(rnd() * 3) + 4;
        // Tree trunk
        setBlock(coord + Vec3i(0, -1, 0), BlockID::DIRT);
        for (int yt = 0; yt != th; yt++)
            setBlock(coord + Vec3i(0, yt, 0), BlockID::WOOD);
        // Tree leaves
        for (int xt = 0; xt != 5; xt++) {
            for (int zt = 0; zt != 5; zt++) {
                for (int yt = 0; yt != 2; yt++) {
                    if (getBlock(coord + Vec3i(xt - 2, th - 3 + yt, zt - 2)) == BlockID::AIR)
                        setBlock(coord + Vec3i(xt - 2, th - 3 + yt, zt - 2), BlockID::LEAF);
                }
            }
        }
        for (int xt = 0; xt != 3; xt++) {
            for (int zt = 0; zt != 3; zt++) {
                for (int yt = 0; yt != 2; yt++) {
                    if (getBlock(coord + Vec3i(xt - 1, th - 1 + yt, zt - 1)) == BlockID::AIR
                        && std::abs(xt - 1) != std::abs(zt - 1))
                        setBlock(coord + Vec3i(xt - 1, th - 1 + yt, zt - 1), BlockID::LEAF);
                }
            }
        }
        setBlock(coord + Vec3i(0, th, 0), BlockID::LEAF);
    }

    void explode(Vec3i center, int r) {
        auto blocks = std::vector<std::pair<Vec3i, BlockID>>();
        // Destroy blocks within a radius of r
        double maxdistsqr = r * r;
        for (int fx = center.x - r - 1; fx < center.x + r + 1; fx++) {
            for (int fy = center.y - r - 1; fy < center.y + r + 1; fy++) {
                for (int fz = center.z - r - 1; fz < center.z + r + 1; fz++) {
                    auto coord = Vec3i(fx, fy, fz);
                    int distsqr = (coord - center).lengthSqr();
                    if (distsqr <= maxdistsqr * 0.75
                        || distsqr <= maxdistsqr && rnd() > (distsqr - maxdistsqr * 0.6) / (maxdistsqr * 0.4)) {
                        BlockID e = World::getBlock(coord);
                        if (!BlockInfo(e).isSolid())
                            continue;
                        setBlock(coord, BlockID::AIR);
                        blocks.emplace_back(Vec3i(coord), e);
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

    void saveAllChunks() {
        for (auto const& [_, c]: chunks)
            c->saveToFile(WorldName);
    }

    struct RenderChunk {
        Vec3i ccoord = {};
        float loadAnim = 0.0f;
        std::array<Renderer::VertexBuffer const*, 2> meshes = {};

        RenderChunk(Chunk const& c, float TimeDelta):
            ccoord(c.coord()),
            loadAnim(c.loadAnimOffset() * std::pow(0.6f, TimeDelta)) {
            meshes[0] = &c.mesh(0);
            meshes[1] = &c.mesh(1);
        }
    };

    auto ListRenderChunks(double x, double y, double z, int distance, double interp, std::optional<FrustumTest> frustum)
        -> std::vector<RenderChunk>;

    void RenderChunks(double x, double y, double z, std::vector<RenderChunk> const& crs, size_t index);

private:
    struct LoadedCore {
        Vec3i ccenter = Vec3i(0, 0, 0);
        size_t radius = 0;
    };

    ChunkID chunkPtrCacheKey = 0;
    Chunk* chunkPtrCacheValue = nullptr;
    ChunkPtrArray chunkPtrArray;
    HeightMap heightMap;
    LoadedCore loadedCore;
};

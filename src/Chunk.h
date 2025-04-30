#pragma once
#include "Definitions.h"
#include "Hitbox.h"
#include "Blocks.h"
#include "FrustumTest.h"
#include "Renderer.h"

namespace World {

class Chunk {
private:
    int cx, cy, cz;
    ChunkID cid;

    std::array<BlockID, 4096> blocks;
    std::array<Brightness, 4096> brightness;

    bool isEmpty = false;
    bool isUpdated = false;
    bool isModified = false;
    bool isDetailGenerated = false;
    bool isMeshed = false;

    std::vector<Renderer::VertexBuffer> meshes;
    float loadAnim = 0.0f;

    void buildTerrain();
    void buildDetail();
    void build();
    std::string getChunkPath() const;

public:
    Chunk(int cx, int cy, int cz, ChunkID cid);
    ~Chunk();

    int x() const { return cx; }
    int y() const { return cy; }
    int z() const { return cz; }
    ChunkID id() const { return cid; }

    // Hint of content
    bool empty() const { return isEmpty; }
    // Render is dirty
    bool updated() const { return isUpdated; }
    // Disk save is dirty
    bool modified() const { return isModified; }
    // All details generated
    bool ready() const { return true; /* return isDetailGenerated; */ }
    // Meshes are available
    bool meshed() const { return isMeshed; }

    BlockID getBlock(size_t x, size_t y, size_t z) const;
    Brightness getBrightness(size_t x, size_t y, size_t z) const;
    void setBlock(size_t x, size_t y, size_t z, BlockID value);
    void setBrightness(size_t x, size_t y, size_t z, Brightness value);

    bool loadFromFile();
    bool saveToFile();
    void buildMeshes();
    void destroyMeshes();
    void markNeighborUpdated() { isUpdated = true; }

    float loadAnimOffset() const { return loadAnim; }
    void updateLoadAnimOffset() {
        if (loadAnim <= 0.3f) loadAnim = 0.0f;
        else loadAnim *= 0.6f;
    }

    Renderer::VertexBuffer const& mesh(size_t index) const {
        assert(index < meshes.size());
        return meshes[index];
    }

    Hitbox::AABB baseAABB() const;
    FrustumTest::AABBf relativeAABB(Vec3d const& orig) const;
    bool visible(Vec3d const& orig, FrustumTest const& frus) const { return frus.test(relativeAABB(orig)); }
};
}

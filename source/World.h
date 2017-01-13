#pragma once
#include "Definitions.h"
#include "chunkPtrArray.h"
#include "HeightMap.h"
#include "Chunk.h"
#include "Hitbox.h"
#include "math/vector.h"

namespace world
{

    extern string worldname;
    const int worldsize = 134217728;
    const int worldheight = 128;
    extern brightness skylight;         //Sky light level
    extern brightness BRIGHTNESSMAX;    //Maximum brightness
    extern brightness BRIGHTNESSMIN;    //Mimimum brightness
    extern brightness BRIGHTNESSDEC;    //Brightness decree
    extern chunk *EmptyChunkPtr;
    extern unsigned int EmptyBuffer;
    extern int MaxChunkLoads;
    extern int MaxChunkUnloads;
    extern int MaxChunkRenders;

    extern chunk **chunks;
    extern int loadedChunks, chunkArraySize;
    extern chunk *cpCachePtr;
    extern chunkid cpCacheID;
    extern HeightMap HMap;
    extern ChunkArrayCache cpArray;

    extern int cloud[128][128];
    extern int rebuiltChunks, rebuiltChunksCount;
    extern int updatedChunks, updatedChunksCount;
    extern int unloadedChunks, unloadedChunksCount;
    extern int chunkBuildRenderList[256][2];
    extern int chunkLoadList[256][4];
    extern pair<chunk *, int> chunkUnloadList[256];
    extern vector<unsigned int> vbuffersShouldDelete;
    extern int chunkBuildRenders, chunkLoads, chunkUnloads;

    void Init();

    chunk *AddChunk(int x, int y, int z);
    void DeleteChunk(int x, int y, int z);
    constexpr size_t getChunkID(const Vec3i &t) noexcept
    {
        return static_cast<size_t>(t.x * 23947293731 + t.z * 3296467037 + t.y * 1234577);
    }
    int getChunkPtrIndex(int x, int y, int z);
    chunk *getChunkPtr(int x, int y, int z);
    void ExpandChunkArray(int cc);
    void ReduceChunkArray(int cc);

#define getchunkpos(n) ((n)>>4)
#define getblockpos(n) ((n)&15)
    inline bool chunkOutOfBound(int x, int y, int z)
    {
        return y < -world::worldheight || y > world::worldheight - 1 ||
               x < -134217728 || x > 134217727 || z < -134217728 || z > 134217727;
    }
    inline bool chunkLoaded(int x, int y, int z)
    {
        if (chunkOutOfBound(x, y, z))
        {
            return false;
        }

        if (getChunkPtr(x, y, z) != nullptr)
        {
            return true;
        }

        return false;
    }

    vector<Hitbox::AABB> getHitboxes(Hitbox::AABB box);
    bool inWater(Hitbox::AABB box);

    void renderblock(int x, int y, int z, chunk *chunkptr);
    void updateblock(int x, int y, int z, bool blockchanged);
    block getblock(int x, int y, int z, block mask = blocks::AIR, chunk *cptr = nullptr);
    brightness getbrightness(int x, int y, int z, chunk *cptr = nullptr);
    void setblock(int x, int y, int z, block Block);
    void setbrightness(int x, int y, int z, brightness Brightness);
    inline void putblock(int x, int y, int z, block Block)
    {
        setblock(x, y, z, Block);
    }
    inline void pickblock(int x, int y, int z)
    {
        setblock(x, y, z, blocks::AIR);
    }

    inline bool chunkInRange(int x, int y, int z, int px, int py, int pz, int dist)
    {
        //检测给出的chunk坐标是否在渲染范围内
        if (x < px - dist || x > px + dist - 1 || y < py - dist || y > py + dist - 1 || z < pz - dist || z > pz + dist - 1)
        {
            return false;
        }

        return true;
    }
    bool chunkUpdated(int x, int y, int z);
    void setChunkUpdated(int x, int y, int z, bool value);
    void sortChunkBuildRenderList(int xpos, int ypos, int zpos);
    void sortChunkLoadUnloadList(int xpos, int ypos, int zpos);

    void saveAllChunks();
    void destroyAllChunks();

    void buildtree(int x, int y, int z);
}

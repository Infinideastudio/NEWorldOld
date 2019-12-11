#pragma once

#include "Definitions.h"
#include "ChunkPtrArray.h"
#include "HeightMap.h"
#include "Chunk.h"
#include "Hitbox.h"
#include "Blocks.h"
#include "Player.h"
#include "Particles.h"
#include "Items.h"

extern int viewdistance;

class Frsutum;

namespace World {

    extern std::string worldname;
    const int worldsize = 134217728;
    const int worldheight = 128;
    extern Brightness skylight;         //Sky light level
    extern Brightness BRIGHTNESSMAX;    //Maximum brightness
    extern Brightness BRIGHTNESSMIN;    //Mimimum brightness
    extern Brightness BRIGHTNESSDEC;    //Brightness decree
    extern Chunk *EmptyChunkPtr;
    extern unsigned int EmptyBuffer;
    extern int MaxChunkLoads;
    extern int MaxChunkUnloads;
    extern int MaxChunkRenders;

    extern Chunk **chunks;
    extern int loadedChunks, chunkArraySize;
    extern Chunk *cpCachePtr;
    extern chunkid cpCacheID;
    extern HeightMap HMap;
    extern ChunkPtrArray cpArray;

    extern int cloud[128][128];
    extern int rebuiltChunks, rebuiltChunksCount;
    extern int updatedChunks, updatedChunksCount;
    extern int unloadedChunks, unloadedChunksCount;
    extern int chunkBuildRenderList[256][2];
    extern int chunkLoadList[256][4];
    extern std::pair<Chunk *, int> chunkUnloadList[256];
    extern std::vector<unsigned int> vbuffersShouldDelete;
    extern int chunkBuildRenders, chunkLoads, chunkUnloads;

    template <class T>
    constexpr T GetChunkPos(const T n) noexcept { return n >> 4; }

    template <class T>
    constexpr T GetBlockPos(const T n) noexcept { return n & 15; }

    void Init();

    Chunk *AddChunk(int x, int y, int z);

    void DeleteChunk(int x, int y, int z);

    inline chunkid GetChunkId(Int3 vec) noexcept {
        if (vec.Y == -128) vec.Y= 0;
        if (vec.Y <= 0) vec.Y = abs(vec.Y) + (1LL << 7);
        if (vec.X == -134217728) vec.X = 0;
        if (vec.X <= 0) vec.X = abs(vec.X) + (1LL << 27);
        if (vec.Z  == -134217728) vec.Z  = 0;
        if (vec.Z  <= 0) vec.Z  = abs(vec.Z ) + (1LL << 27);
        return (chunkid(vec.Y) << 56) + (chunkid(vec.X) << 28) + vec.Z ;
    }

    int GetChunkIndex(int x, int y, int z);

    Chunk *GetChunk(Int3 vec);

    void ExpandChunkArray(int cc);

    void ReduceChunkArray(int cc);

    constexpr bool ChunkOutOfBound(const Int3 v) noexcept {
        return v.Y < -World::worldheight || v.Y > World::worldheight - 1 ||
                v.X < -134217728 || v.X > 134217727 || v.Z < -134217728 || v.Z > 134217727;
    }

    inline bool ChunkLoaded(const Int3 v) noexcept {
        if (ChunkOutOfBound(v)) return false;
        return GetChunk(v);
    }

    std::vector<Hitbox::AABB> getHitboxes(const Hitbox::AABB &box);

    bool inWater(const Hitbox::AABB &box);

    void renderblock(int x, int y, int z, Chunk *chunkptr);

    void updateblock(int x, int y, int z, bool blockchanged, int depth = 0);

    Block GetBlock(Int3 v, Block mask = Blocks::AIR, Chunk *hint = nullptr);

    Brightness GetBrightness(Int3 v, Chunk *hint = nullptr);

    void SetBlock(Int3 v, Block block, Chunk *hint = nullptr);

    void SetBrightness(Int3 v, Brightness brightness, Chunk *hint = nullptr);

    inline Brightness getbrightness(int x, int y, int z, Chunk *cptr = nullptr) {
        return GetBrightness({x, y, z}, cptr);
    }

    inline void putblock(int x, int y, int z, Block Block) { SetBlock({(x), (y), (z)}, Block); }

    inline void pickleaf() {
        if (rnd() < 0.2) {
            if (rnd() < 0.5)Player::addItem(APPLE);
            else Player::addItem(STICK);
        } else {
            Player::addItem(Blocks::LEAF);
        }
    }

    inline void picktree(int x, int y, int z) {
        if (GetBlock({(x), (y), (z)}) != Blocks::LEAF) {
            Player::addItem(GetBlock({(x), (y), (z)}));
        }
        else pickleaf();
        for (int j = 1; j <= 10; j++) {
            Particles::throwParticle(GetBlock({(x), (y), (z)}),
                                     float(x + rnd() - 0.5f), float(y + rnd() - 0.2f), float(z + rnd() - 0.5f),
                                     float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f), float(rnd() * 0.2f - 0.1f),
                                     float(rnd() * 0.02 + 0.03), int(rnd() * 60) + 30);
        }
        SetBlock({(x), (y), (z)}, Blocks::AIR);
        //上
        if ((GetBlock({(x), (y + 1), (z)}) == Blocks::WOOD) || (
                GetBlock({(x), (y + 1), (z)}) == Blocks::LEAF))picktree(x, y + 1, z);
        //前
        if ((GetBlock({(x), (y), (z + 1)}) == Blocks::WOOD) || (
                GetBlock({(x), (y), (z + 1)}) == Blocks::LEAF))picktree(x, y, z + 1);
        //后
        if ((GetBlock({(x), (y), (z - 1)}) == Blocks::WOOD) || (
                GetBlock({(x), (y), (z - 1)}) == Blocks::LEAF))picktree(x, y, z - 1);
        //左
        if ((GetBlock({(x + 1), (y), (z)}) == Blocks::WOOD) || (
                GetBlock({(x + 1), (y), (z)}) == Blocks::LEAF))picktree(x + 1, y, z);
        //右
        if ((GetBlock({(x - 1), (y), (z)}) == Blocks::WOOD) || (
                GetBlock({(x - 1), (y), (z)}) == Blocks::LEAF))picktree(x - 1, y, z);
    }

    inline void pickblock(int x, int y, int z) {
        if (GetBlock({(x), (y), (z)}) == Blocks::WOOD &&
            ((GetBlock({(x), (y + 1), (z)}) == Blocks::WOOD) || (
                    GetBlock({(x), (y + 1), (z)}) == Blocks::LEAF)) &&
            (GetBlock({(x), (y), (z + 1)}) == Blocks::AIR) && (
                    GetBlock({(x), (y), (z - 1)}) == Blocks::AIR) &&
            (GetBlock({(x + 1), (y), (z)}) == Blocks::AIR) && (
                    GetBlock({(x - 1), (y), (z)}) == Blocks::AIR) &&
            (GetBlock({(x), (y - 1), (z)}) != Blocks::AIR)
                ) { picktree(x, y + 1, z); }//触发砍树模式
        //击打树叶
        if (GetBlock({(x), (y), (z)}) != Blocks::LEAF) {
            Player::addItem(GetBlock({(x), (y), (z)}));
        }
        else pickleaf();

        SetBlock({(x), (y), (z)}, Blocks::AIR);
    }


    inline bool chunkInRange(int x, int y, int z, int px, int py, int pz, int dist) {
        //检测给出的chunk坐标是否在渲染范围内
        return !(x < px - dist || x > px + dist - 1 || y < py - dist || y > py + dist - 1 || z < pz - dist ||
                 z > pz + dist - 1);
    }

    bool chunkUpdated(Int3 vec);

    void setChunkUpdated(int x, int y, int z, bool value);

    void sortChunkBuildRenderList(int xpos, int ypos, int zpos);

    void sortChunkLoadUnloadList(int xpos, int ypos, int zpos);

    void calcVisible(double xpos, double ypos, double zpos, Frustum &frus);

    void saveAllChunks();

    void destroyAllChunks();

    void buildtree(int x, int y, int z);

    void explode(int x, int y, int z, int r, Chunk *c = nullptr);
}

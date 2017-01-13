#pragma once
#include "Definitions.h"
#include "HeightMap.h"
#include "Chunk.h"
#include "Hitbox.h"
#include "math/vector.h"
#include <array>
#include <unordered_map>
namespace world
{

    template <class Tk, class Td, size_t size, template<class>class Compare = std::less>
    class OrderedList
    {
    public:
        OrderedList() noexcept : mComp(), mSize(0) {}
        using ArrayType = std::array<std::pair<Tk, Td>, size>;
        using Iterator = typename ArrayType::iterator;
        using ConstIterator = typename ArrayType::const_iterator;
        Iterator begin() noexcept { return mList.begin(); }
        ConstIterator begin() const noexcept { return mList.begin(); }
        Iterator end() noexcept { return mList.begin() + mSize; }
        ConstIterator end() const noexcept { return mList.begin() + mSize; }
        void insert(Tk key, Td data) noexcept
        {
            int first = 0, last = mSize - 1;
            while (first <= last)
            {
                int middle = (first + last) / 2;
                if (mComp(key, mList[middle].first))
                    last = middle - 1;
                else
                    first = middle + 1;
            }
            if (first <= mSize && first < size)
            {
                mSize = std::min(size, mSize + 1);
                for (int j = size - 1; j > first; j--)
                    mList[j] = mList[j - 1];
                mList[first] = std::pair<Tk, Td>(key, data);
            }
        }
        void clear() noexcept { mSize = 0; }
    private:
        size_t mSize;
        ArrayType mList;
        Compare<Tk> mComp;
    };

    constexpr size_t getChunkID(const Vec3i &t) noexcept
    {
        return static_cast<size_t>(t.x * 23947293731 + t.z * 3296467037 + t.y * 1234577);
    }
    class World
    {
    public:
        auto begin() noexcept { return mChunks.begin(); }
        auto end() noexcept { return mChunks.end(); }
        auto begin() const noexcept { return mChunks.begin(); }
        auto end() const noexcept { return mChunks.end(); }

        friend chunk *AddChunk(int x, int y, int z);
        friend void DeleteChunk(int x, int y, int z);
        friend chunk *getChunkPtr(int x, int y, int z);
    private:
        struct ChunkHash
        {
            constexpr size_t operator()(const Vec3i &t) const noexcept { return getChunkID(t); }
        };
        std::unordered_map<Vec3i, chunk, ChunkHash> mChunks;
    };
    extern World mWorld;
    extern string worldname;
    const int worldsize = 134217728;
    const int worldheight = 128;
    extern brightness skylight;         //Sky light level
    extern brightness BRIGHTNESSMAX;    //Maximum brightness
    extern brightness BRIGHTNESSMIN;    //Mimimum brightness
    extern brightness BRIGHTNESSDEC;    //Brightness decree
    extern unsigned int EmptyBuffer;
    constexpr int MaxChunkLoads = 64;
    constexpr int MaxChunkUnloads = 64;
    constexpr int MaxChunkRenders = 4;

    extern HeightMap HMap;

    extern int cloud[128][128];
    extern OrderedList<int, Vec3i, MaxChunkLoads> chunkLoadList;
    extern OrderedList<int, chunk*, MaxChunkRenders> chunkBuildRenderList;
    extern OrderedList<int, chunk*, MaxChunkUnloads, std::greater> chunkUnloadList;
    extern vector<unsigned int> vbuffersShouldDelete;

    void Init();

    chunk *AddChunk(int x, int y, int z);
    void DeleteChunk(int x, int y, int z);
    chunk *getChunkPtr(int x, int y, int z);

#define getchunkpos(n) ((n)>>4)
#define getblockpos(n) ((n)&15)
    inline bool chunkLoaded(int x, int y, int z)
    {
        return getChunkPtr(x, y, z) != nullptr;
    }

    vector<Hitbox::AABB> getHitboxes(Hitbox::AABB box);
    bool inWater(Hitbox::AABB box);

    void renderblock(int x, int y, int z, chunk *chunkptr);
    void updateblock(int x, int y, int z, bool blockchanged);
    block getblock(int x, int y, int z, block mask = blocks::NONEMPTY, chunk *cptr = nullptr);
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

    //检测给出的chunk坐标是否在渲染范围内
    inline bool chunkInRange(int x, int y, int z, int px, int py, int pz, int dist)
    {
        return (x <= px + dist && x >= px - dist && y <= py + dist && y >= py - dist && z <= pz + dist && z >= pz - dist);
    }
    bool chunkUpdated(int x, int y, int z);
    void setChunkUpdated(int x, int y, int z, bool value);
    void sortChunkBuildRenderList(int xpos, int ypos, int zpos);
    void sortChunkLoadUnloadList(int xpos, int ypos, int zpos);

    void saveAllChunks();
    void destroyAllChunks();

    void buildtree(int x, int y, int z);
}

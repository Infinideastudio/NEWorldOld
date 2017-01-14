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
    template <class Tk, class Td, size_t Size, template<class>class Compare = std::less>
    class OrderedList
    {
    public:
        OrderedList() noexcept : mComp(), mSize(0) {}
        using ArrayType = std::array<std::pair<Tk, Td>, Size>;
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
            if (first <= mSize && first < Size)
            {
                mSize = std::min(Size, mSize + 1);
                std::move_backward(mList.begin() + first, mList.begin() +Size-1, mList.begin()+Size);
                mList[first] = std::pair<Tk, Td>(key, data);
            }
        }
        void clear() noexcept { mSize = 0; }
        void move_forward(Iterator iter)
        {
            std::move(iter, end(), begin());
            mSize -= iter-begin();
        }
        size_t size() noexcept {return mSize; }
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

        chunk *insertChunk(int x, int y, int z);
        void eraseChunk(int x, int y, int z);
        friend chunk *getChunkPtr(int x, int y, int z);
        void tryLoadUnloadChunks(const Vec3i& centre);
        void tryUpdateRenderers(const Vec3i& centre);
    private:
        constexpr static size_t LazyUpdateDistanceSquare = 256;
        constexpr static size_t EmptyChunkCacheSize = 1024;
        constexpr static double LazyUpdateMaxTime = 3.0;
        constexpr static double UpdateMaxTime = 0.02;
        constexpr static size_t MaxChunkLoads = 128;
        constexpr static size_t MaxChunkUnloads = 128;
        constexpr static size_t MaxChunkRenders = 64;
        struct ChunkHash
        {
            constexpr size_t operator()(const Vec3i &t) const noexcept { return getChunkID(t); }
        };
        std::string mName;
        std::unordered_map<Vec3i, chunk, ChunkHash> mChunks;
    };
    extern World mWorld;
    extern string worldname;
    extern brightness skylight;         //Sky light level
    extern brightness BRIGHTNESSMAX;    //Maximum brightness
    extern brightness BRIGHTNESSMIN;    //Mimimum brightness
    extern brightness BRIGHTNESSDEC;    //Brightness degree

    extern HeightMap HMap;

    extern vector<unsigned int> vbuffersShouldDelete;

    void Init();

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
    //检测给出的chunk坐标是否在渲染范围内
    inline bool chunkInRange(int x, int y, int z, int px, int py, int pz, int dist)
    {
        return (x <= px + dist && x >= px - dist && y <= py + dist && y >= py - dist && z <= pz + dist && z >= pz - dist);
    }
    bool chunkUpdated(int x, int y, int z);
    void setChunkUpdated(int x, int y, int z, bool value);
    void saveAllChunks();
    void destroyAllChunks();

    void buildtree(int x, int y, int z);
}

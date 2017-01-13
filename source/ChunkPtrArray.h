#pragma once
#include "Definitions.h"
#include "Chunk.h"
#include <memory>

namespace world
{
    class ChunkArrayCache
    {
    public:
        using Pointer = chunk*;
        int originX, originY, originZ;

        ChunkArrayCache() = default;
        ~ChunkArrayCache() = default;
        ChunkArrayCache(ChunkArrayCache &&) = default;
        ChunkArrayCache(const ChunkArrayCache &) = delete;
        ChunkArrayCache &operator = (ChunkArrayCache &&) = default;
        ChunkArrayCache &operator = (const ChunkArrayCache &) = delete;

        ChunkArrayCache(int s) :
            mSize(s), mSize2(s * s), mSize3(s * s * s), mCache(new Pointer[s * s * s])
        {
            std::fill(mCache.get(), mCache.get() + s * s * s, nullptr);
        }
        void moveTo(int x, int y, int z)
        {
            move(x - originX, y - originY, z - originZ);
        }
        void set(chunk *cptr, int cx, int cy, int cz) noexcept
        {
            cx -= originX;
            cy -= originY;
            cz -= originZ;

            if (exists(cx, cy, cz))
            {
                mCache[cx * mSize2 + cy * mSize + cz] = cptr;
            }
        }

        void erase(int cx, int cy, int cz) noexcept
        {
            cx -= originX;
            cy -= originY;
            cz -= originZ;

            if (exists(cx, cy, cz))
            {
                mCache[cx * mSize2 + cy * mSize + cz] = nullptr;
            }
        }

        bool exists(int x, int y, int z) const noexcept
        {
            return x >= 0 && x < mSize && z >= 0 && z < mSize && y >= 0 && y < mSize;
        }

        chunk *get(int x, int y, int z) noexcept
        {
            x -= originX;
            y -= originY;
            z -= originZ;
            return exists(x, y, z) ? mCache[x * mSize2 + y * mSize + z] : nullptr;
        }
    private:
        std::unique_ptr<Pointer[]> mCache{ nullptr };
        int mSize, mSize2, mSize3;
        void move(int xd, int yd, int zd)
        {
            std::unique_ptr<Pointer[]> arrTemp{ new Pointer[mSize3] };

            for (int x = 0; x < mSize; x++)
                for (int y = 0; y < mSize; y++)
                    for (int z = 0; z < mSize; z++)
                    {
                        arrTemp[x * mSize2 + y * mSize + z] = exists(x + xd, y + yd, z + zd) ? mCache[(x + xd) * mSize2 + (y + yd) * mSize + (z + zd)] : nullptr;
                    }

            mCache.swap(arrTemp);
            originX += xd;
            originY += yd;
            originZ += zd;
        }
    };
}

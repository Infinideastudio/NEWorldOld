#pragma once
#include "Definitions.h"
#include "Math/Vector3.h"

namespace World {
    class Chunk;
    class ChunkPtrArray {
    public:
        void Create(int s);

        void Finalize();

        void Move(const Int3& delta);

        void MoveTo(const Int3& pos) { Move(pos - origin); }

        void Add(Chunk* c, const Int3& pos) noexcept { Set(pos, c); }

        void Remove(const Int3& pos) noexcept { Set(pos, nullptr); }

        Chunk* Get(const Int3& pos) const noexcept { return Fetch(pos-origin); }

        void Set(const Int3& pos, Chunk* c) noexcept { Write(pos-origin, c); }
    private:
        bool Has(const Int3& pos) const noexcept {
            return pos.X>=0 && pos.X<size && pos.Z>=0 && pos.Z<size && pos.Y>=0 && pos.Y<size;
        }

        Chunk* Fetch(const Int3& pos) const noexcept { return Has(pos) ? array[Dot(pos, man)] : nullptr; }

        void Write(const Int3& pos, Chunk* c) noexcept { if (Has(pos)) array[Dot(pos, man)] = c; }

        Chunk** array = nullptr;
        Int3 origin, man;
        int size, size2, size3;
    };
}

#pragma once

#include "Math/Vector3.h"

namespace World {
    class Chunk;

    class ChunkPtrArray {
    public:
        void Create(int s);

        void Finalize();

        void Move(const Int3 &delta);

        void MoveTo(const Int3 &pos) { Move(pos - origin); }

        void Add(Chunk *c, const Int3 &pos) noexcept { Set(pos, c); }

        void Remove(const Int3 &pos) noexcept { Set(pos, nullptr); }

        [[nodiscard]] Chunk *Get(const Int3 &pos) const noexcept { return Fetch(pos - origin); }

        void Set(const Int3 &pos, Chunk *c) noexcept { Write(pos - origin, c); }

    private:
        [[nodiscard]] bool Has(const Int3 v) const noexcept {
            return v.X >= 0 && v.X < size && v.Z >= 0 && v.Z < size && v.Y >= 0 && v.Y < size;
        }

        [[nodiscard]] Chunk *Fetch(const Int3 v) const noexcept { return Has(v) ? array[Dot(v, man)] : nullptr; }

        void Write(const Int3 &pos, Chunk *c) noexcept { if (Has(pos)) array[Dot(pos, man)] = c; }

        Chunk **array = nullptr;
        Int3 origin{}, man{};
        int size{}, size2{}, size3{};
    };
}

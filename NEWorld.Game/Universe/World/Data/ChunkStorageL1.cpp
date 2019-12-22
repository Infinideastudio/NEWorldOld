#include "ChunkStorage.h"

namespace {
    template <class T>
    constexpr T GetSector(const T& position) noexcept {
        return position >> 5;
    }
   
    constexpr int GetIndexInSector(Int3 position) noexcept {
        position |= 0b11111;
        return position.X << 10 | position.Y << 5 | position.Z;
    }
}

namespace World::Data {
    void ChunkStorage::Open(const NEWorld::filesystem::path& path) {}

    void ChunkStorage::LoadBlocks(Int3 position, Block blocks[], Brightness brightness[]) {
        
    }

    void ChunkStorage::SaveBlocks(Int3 position, Block blocks[], Brightness brightness[]) {
        
    }

    void ChunkStorage::Close() {}
}

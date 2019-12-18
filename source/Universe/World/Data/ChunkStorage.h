#pragma once

#include <filesystem>
#include "Math/Vector3.h"
#include "Universe/World/BlockRegistry.h"

namespace World::Data {
    struct ChunkStorage {
        void Open(const std::filesystem::path& path);

        void LoadBlocks(Int3 position, Block blocks[], Brightness brightness[]);

        void SaveBlocks(Int3 position, Block blocks[], Brightness brightness[]);

        void Close();
    };
}

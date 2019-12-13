#pragma once

#include "Blocks.h"
#include <filesystem>

class BlockRegistry {
public:
    static void Register(Blocks::BlockType* type);

    static bool LoadIdTable(const std::filesystem::path& path);

    static void CreateIdTableNull();

    static void CreateIdTableAuto();

    static bool AppendIdTable(const std::string_view &name);

    static bool SaveIdTable(const std::filesystem::path& path);

    static void ClearIdTable() noexcept;

    static Blocks::BlockType* QueryTable(int id) noexcept;

    static int QueryId(const std::string_view& name) noexcept;
};

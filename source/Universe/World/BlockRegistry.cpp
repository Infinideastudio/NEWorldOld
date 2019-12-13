#include "BlockRegistry.h"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <fstream>

namespace {
    std::once_flag gEnvBlockInit;
    std::vector<Blocks::BlockType*> gTable;
    std::unordered_map<std::string_view, std::pair<Blocks::BlockType*, int>> gTypes;
    auto gEnvBlock = Blocks::BlockType("NEWorld.Blocks.Air", false, false, false, 0); // NOLINT
    constexpr const char* gConfigName = "new_world_block_id_table";

    void TableExtract(const nlohmann::json& cfgTree) {
        auto&& entries = cfgTree[gConfigName];
        gTable.resize(entries.size());
        for (auto& [key, value] : entries.items()) {
            std::string name = key;
            const auto iter = gTypes.find(name);
            if (iter != gTypes.end()) {
                const auto index = static_cast<int>(value);
                iter->second.second = index;
                gTable[index] = iter->second.first;
            }
            else {
                throw std::runtime_error("Required Block: (" + name + ") Not Found");
            }
        }
    }

    void TableDump(nlohmann::json& cfgTree) {
        for (const auto& x : gTypes) {
            cfgTree[std::string(x.first)] = x.second.second;
        }
    }
}

void BlockRegistry::Register(Blocks::BlockType *type) {
    if (type) {
        gTypes.insert_or_assign(type->GetId(), std::pair{type, -1});
    }
    std::call_once(gEnvBlockInit, []() {
        gTypes.insert_or_assign(gEnvBlock.GetId(), std::pair{&gEnvBlock, 0});
    });
}

bool BlockRegistry::LoadIdTable(const std::filesystem::path &path) {
    ClearIdTable();
    std::ifstream config { path };
    if (config) {
        nlohmann::json cfgTree {};
        config >> cfgTree;
        TableExtract(cfgTree);
        return true;
    }
    return false;
}

void BlockRegistry::CreateIdTableNull() {
    ClearIdTable();
    gTable.emplace_back(&gEnvBlock);
}

void BlockRegistry::CreateIdTableAuto() {
    CreateIdTableNull();
    for (auto& [_, pair] : gTypes) {
        if (pair.second) {
            pair.second = gTable.size();
            gTable.emplace_back(pair.first);
        }
    }
}

bool BlockRegistry::AppendIdTable(const std::string_view &name) {
    const auto iter = gTypes.find(name);
    if (iter != gTypes.end()) {
        auto& [type, id] = iter->second;
        if (id == -1) {
            id = gTable.size();
            gTable.emplace_back(type);
            return true;
        }
    }
    return false;
}

bool BlockRegistry::SaveIdTable(const std::filesystem::path &path) {
    std::ofstream config { path };
    if (config) {
        nlohmann::json cfgTree {};
        TableDump(cfgTree);
        config << cfgTree;
        return true;
    }
    return false;
}

Blocks::BlockType *BlockRegistry::QueryTable(int id) noexcept { return gTable[id]; }

int BlockRegistry::QueryId(const std::string_view &name) noexcept {
    const auto iter = gTypes.find(name);
    return iter != gTypes.end() ? iter->second.second: -1;
}

void BlockRegistry::ClearIdTable() noexcept {
    if (!gTable.empty()) {
        for (auto &x : gTypes) { x.second.second = -1; }
        gTable.clear();
    }
}

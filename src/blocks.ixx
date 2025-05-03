module;

#include <cassert>
#include <optional>
#include <string>
#include <vector>

export module blocks;
import types;

// Block data that are actually stored in chunks.
export class BlockData {
public:
    // A wrapper for block ID. The default is 0.
    class Id {
    public:
        static constexpr auto MIN_VALUE = uint16_t{0x0000};
        static constexpr auto MAX_VALUE = uint16_t{0xFFFF};

        constexpr Id() = default;
        explicit constexpr Id(uint16_t value):
            _data(value) {}

        auto get() const -> uint16_t {
            return _data;
        }
        auto operator==(Id const& r) const -> bool = default;

    private:
        uint16_t _data = 0;
    };

    // A wrapper for block state. The default is 0, not empty.
    // An empty value intends to denote that the state is stored externally (e.g. in the chunk).
    // This is currently unused.
    class State {
    public:
        static constexpr auto MIN_VALUE = uint8_t{0x00};
        static constexpr auto MAX_VALUE = uint8_t{0xFE};

        constexpr State() = default;
        explicit constexpr State(std::optional<uint8_t> value):
            _data(value.value_or(0xFF)) {}

        auto get() const -> std::optional<uint8_t> {
            return _data < 0xFF ? std::optional<uint8_t>(_data) : std::nullopt;
        }
        auto operator==(State const& r) const -> bool = default;

    private:
        uint16_t _data = 0;
    };

    // A wrapper for sky and block light levels. The default is 0 and 0.
    struct Light {
        static constexpr auto SKY_MIN_VALUE = uint8_t{0x00};
        static constexpr auto SKY_MAX_VALUE = uint8_t{0x0F};
        static constexpr auto BLOCK_MIN_VALUE = uint8_t{0x00};
        static constexpr auto BLOCK_MAX_VALUE = uint8_t{0x0F};

        constexpr Light() = default;
        explicit constexpr Light(uint8_t sky_value, uint8_t block_value):
            _data((sky_value << 4) | (block_value & 0x0F)) {}

        auto sky() const -> uint8_t {
            return _data >> 4;
        }
        auto block() const -> uint8_t {
            return _data & 0x0F;
        }
        auto operator==(Light const& r) const -> bool = default;

        // Temporary: mix two light levels.
        auto mixed() const -> float {
            auto skyf = static_cast<float>(sky()) / static_cast<float>(SKY_MAX_VALUE);
            auto blockf = static_cast<float>(block()) / static_cast<float>(BLOCK_MAX_VALUE);
            return std::max(skyf, blockf);
        }

    private:
        uint16_t _data = 0;
    };

    Id id;
    State state;
    Light light;

    // Returns true if all fields (`id`, `state`, `light`) are equal.
    auto operator==(BlockData const& r) const -> bool = default;
};

// Globally stored properties for every block ID.
export class BlockInfo {
public:
    std::string name;
    bool solid;
    bool opaque;
    bool translucent;
    float hardness;
};

// Global block properties registry.
export class BlockInfoRegistry {
public:
    auto add(BlockInfo info) -> BlockData::Id {
        auto id = _entries.size();
        if (id > BlockData::Id::MAX_VALUE)
            assert(false);
        _entries.emplace_back(std::move(info));
        return BlockData::Id(id);
    }

    auto get(BlockData::Id id) const -> BlockInfo const& {
        return id.get() < _entries.size() ? _entries[id.get()] : _DEFAULT_INFO;
    }

private:
    static constexpr auto _DEFAULT_INFO =
        BlockInfo{.name = "null", .solid = true, .opaque = true, .translucent = false, .hardness = 0.0f};

    std::vector<BlockInfo> _entries;
};

// Blocks defined by the base game.
export struct BaseBlocks {
    BlockData::Id air;
    BlockData::Id rock;
    BlockData::Id grass;
    BlockData::Id dirt;
    BlockData::Id stone;
    BlockData::Id plank;
    BlockData::Id wood;
    BlockData::Id bedrock;
    BlockData::Id leaf;
    BlockData::Id glass;
    BlockData::Id water;
    BlockData::Id lava;
    BlockData::Id glowstone;
    BlockData::Id sand;
    BlockData::Id cement;
    BlockData::Id ice;
    BlockData::Id coal;
    BlockData::Id iron;
    BlockData::Id tnt;
};

// Registers all base blocks to the given registry.
export auto register_base_blocks(BlockInfoRegistry& r) -> BaseBlocks {
    auto res = BaseBlocks{};
    res.air = r.add({.name = "air", .solid = false, .opaque = false, .translucent = false, .hardness = 0.0f});
    res.rock = r.add({.name = "rock", .solid = true, .opaque = true, .translucent = false, .hardness = 2.0f});
    res.grass = r.add({.name = "grass", .solid = true, .opaque = true, .translucent = false, .hardness = 0.3f});
    res.dirt = r.add({.name = "dirt", .solid = true, .opaque = true, .translucent = false, .hardness = 0.3f});
    res.stone = r.add({.name = "stone", .solid = true, .opaque = true, .translucent = false, .hardness = 1.0f});
    res.plank = r.add({.name = "plank", .solid = true, .opaque = true, .translucent = false, .hardness = 1.0f});
    res.wood = r.add({.name = "wood", .solid = true, .opaque = true, .translucent = false, .hardness = 2.0f});
    res.bedrock = r.add({.name = "bedrock", .solid = true, .opaque = true, .translucent = false, .hardness = 10.0f});
    res.leaf = r.add({.name = "leaf", .solid = true, .opaque = false, .translucent = false, .hardness = 0.2f});
    res.glass = r.add({.name = "glass", .solid = true, .opaque = false, .translucent = false, .hardness = 0.2f});
    res.water = r.add({.name = "water", .solid = false, .opaque = false, .translucent = true, .hardness = 0.0f});
    res.lava = r.add({.name = "lava", .solid = false, .opaque = false, .translucent = true, .hardness = 0.0f});
    res.glowstone =
        r.add({.name = "glow stone", .solid = true, .opaque = true, .translucent = false, .hardness = 1.0f});
    res.sand = r.add({.name = "sand", .solid = true, .opaque = true, .translucent = false, .hardness = 0.2f});
    res.cement = r.add({.name = "cement", .solid = true, .opaque = true, .translucent = false, .hardness = 3.0f});
    res.ice = r.add({.name = "ice", .solid = true, .opaque = false, .translucent = true, .hardness = 0.2f});
    res.coal = r.add({.name = "coal block", .solid = true, .opaque = true, .translucent = false, .hardness = 0.2f});
    res.iron = r.add({.name = "iron block", .solid = true, .opaque = true, .translucent = false, .hardness = 3.0f});
    res.tnt = r.add({.name = "tnt", .solid = true, .opaque = true, .translucent = false, .hardness = 0.2f});
    return res;
}

// ===== Temporary: compatibility interface with the old code. =====
BlockInfoRegistry block_info_registry;
BaseBlocks base_blocks;

export auto initBlocks() -> void {
    base_blocks = register_base_blocks(block_info_registry);
}

export auto Blocks() -> BaseBlocks const& {
    return base_blocks;
}

export auto BlockInfo(BlockData::Id id) -> BlockInfo const& {
    return block_info_registry.get(id);
}

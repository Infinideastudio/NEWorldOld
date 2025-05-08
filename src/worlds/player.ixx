module;

#include <spdlog/spdlog.h>

export module worlds:player;
import std;
import types;
import math;
import chunks;
import blocks;
import globals;
import items;
import :forward;

namespace player {

export class Player {
public:
    enum class GameMode { SURVIVAL, CREATIVE };

    Player() = default;
    Player(std::string_view world_name);

    auto save(std::string_view world_name) const -> bool;

    // Getters and setters for encapsulated fields
    auto coord() const -> Vec3d {
        return _coord;
    }

    auto look_coord() const -> Vec3d {
        return {_coord.x(), _coord.y() + LOOK_HEIGHT, _coord.z()};
    }

    auto aabb() const -> AABB3d {
        return {
            _coord - Vec3d(0.3, 0.0, 0.3),
            _coord + Vec3d(0.3, 1.7, 0.3),
        };
    }

    void set_coord(Vec3d value) {
        _coord = value;
        _velocity = 0.0;
    }

    auto velocity() const -> Vec3d {
        return _velocity;
    }

    void set_velocity(Vec3d value) {
        _velocity = value;
    }

    auto orientation() const -> Eulerd {
        return _orientation;
    }

    void set_orientation(Eulerd value) {
        constexpr auto PI = std::numbers::pi_v<double>;
        value = value.normalize();
        value.pitch() = std::clamp(value.pitch(), -PI / 2, PI / 2);
        _orientation = value;
    }

    auto flying() const -> bool {
        return _flying;
    }

    auto cross_wall() const -> bool {
        return _cross_wall;
    }

    void set_cross_wall(bool value) {
        switch (_game_mode) {
            case GameMode::SURVIVAL:
                _cross_wall = false;
                break;
            case GameMode::CREATIVE:
                _cross_wall = value;
                break;
        }
    }

    auto grounded() const -> bool {
        return _grounded;
    }

    auto near_wall() const -> bool {
        return _near_wall;
    }

    auto in_water() const -> bool {
        return _in_water;
    }

    auto running() const -> bool {
        return _running;
    }

    auto speed() const -> double {
        return _running ? RUN_SPEED : WALK_SPEED;
    }

    void set_running(bool value) {
        _running = value;
    }

    auto game_mode() const -> GameMode {
        return _game_mode;
    }

    void set_game_mode(GameMode mode) {
        _game_mode = mode;
        switch (_game_mode) {
            case GameMode::SURVIVAL:
                _flying = false;
                _cross_wall = false;
                break;

            case GameMode::CREATIVE:
                _flying = true;
                break;
        }
    }

    auto health() const -> double {
        return _health;
    }

    auto max_health() const -> double {
        return _max_health;
    }

    void set_health(double value) {
        _health = std::clamp(value, 0.0, _max_health);
    }

    auto inventory_item_stack(size_t row, size_t col) -> items::ItemStack& {
        return _inventory[row][col];
    }

    auto held_item_stack_index() const -> size_t {
        return _held_item_stack_index;
    }

    auto held_item_stack() -> items::ItemStack& {
        return _inventory.back()[_held_item_stack_index];
    }

    void set_held_item_stack_index(size_t index) {
        _held_item_stack_index = std::clamp(index, static_cast<size_t>(0), static_cast<size_t>(9));
    }

    void spawn() {
        set_coord({0.0, 128.0, 0.0});
        _health = _max_health;
    }

    void update(worlds::World& world);

    auto put_block(worlds::World& world, Vec3i coord, blocks::Id blockname) -> bool;

    auto add_item(items::ItemStack stack) -> bool;

    void clear_inventory() {
        _inventory = {};
    }

    void on_jump(bool just_pressed) {
        if (!_flying && !_cross_wall) {
            if (!_in_water) {
                if (just_pressed && _jumps < MAX_JUMPS || _grounded) {
                    _jumps++;
                    _grounded = false;
                    _velocity.y() = 0.3;
                }
            } else {
                _velocity.y() = WALK_SPEED;
            }
        } else {
            _velocity.y() += WALK_SPEED;
        }
    }

    void on_crouch() {
        if (!_flying && !_cross_wall) {
            if (!_in_water) {

            } else {
                _velocity.y() = -WALK_SPEED;
            }
        } else {
            _velocity.y() -= WALK_SPEED;
        }
    }

private:
    static constexpr auto WALK_SPEED = 0.25f;
    static constexpr auto RUN_SPEED = 0.5f;
    static constexpr auto MAX_JUMPS = 3;
    static constexpr auto LOOK_HEIGHT = 1.6;

    // Player attributes
    Vec3d _coord = {0.0, 128.0, 0.0};
    Vec3d _velocity = {};
    Eulerd _orientation = {};

    bool _flying = false;
    bool _cross_wall = false;
    bool _grounded = false;
    bool _near_wall = false;
    bool _in_water = false;
    bool _running = false;
    int _jumps = 0;

    GameMode _game_mode = GameMode::SURVIVAL;
    double _health = 20, _max_health = _health, _heal_speed = 0.01, _fall_damage = 5.0;

    std::array<std::array<items::ItemStack, 10>, 4> _inventory = {};
    size_t _held_item_stack_index = 0;
};

Player::Player(std::string_view world_name) {
    spdlog::debug("Loading player data");

    auto path = std::filesystem::path("worlds") / world_name / "player.neworldplayer";
    auto file = std::ifstream(path, std::ios::binary | std::ios::in);
    if (!file.is_open()) {
        spdlog::error("Failed to open player data file");
        throw std::runtime_error("Failed to open player data file");
    }
    auto targetVersion = uint32_t{0};
    file.read(reinterpret_cast<char*>(&targetVersion), sizeof(targetVersion));
    if (targetVersion != GameVersion) {
        spdlog::error("Player data version mismatch: expected {}, got {}", GameVersion, targetVersion);
        throw std::runtime_error("Player data version mismatch");
    }
    file.read(reinterpret_cast<char*>(&_coord), sizeof(_coord));
    file.read(reinterpret_cast<char*>(&_orientation), sizeof(_orientation));
    file.read(reinterpret_cast<char*>(&_grounded), sizeof(_grounded));
    file.read(reinterpret_cast<char*>(&_running), sizeof(_running));
    file.read(reinterpret_cast<char*>(&_jumps), sizeof(_jumps));
    file.read(reinterpret_cast<char*>(&_flying), sizeof(_flying));
    file.read(reinterpret_cast<char*>(&_cross_wall), sizeof(_cross_wall));
    file.read(reinterpret_cast<char*>(&_held_item_stack_index), sizeof(_held_item_stack_index));
    file.read(reinterpret_cast<char*>(&_health), sizeof(_health));
    file.read(reinterpret_cast<char*>(&_game_mode), sizeof(_game_mode));
    file.read(reinterpret_cast<char*>(&GameTime), sizeof(GameTime));
    file.read(reinterpret_cast<char*>(_inventory.data()), sizeof(_inventory));
}

auto Player::save(std::string_view world_name) const -> bool {
    auto path = std::filesystem::path("worlds") / world_name / "player.neworldplayer";
    auto file = std::ofstream(path, std::ios::binary | std::ios::out);
    if (!file.is_open()) {
        return false;
    }
    file.write(reinterpret_cast<char const*>(&GameVersion), sizeof(GameVersion));
    file.write(reinterpret_cast<char const*>(&_coord), sizeof(_coord));
    file.write(reinterpret_cast<char const*>(&_orientation), sizeof(_orientation));
    file.write(reinterpret_cast<char const*>(&_grounded), sizeof(_grounded));
    file.write(reinterpret_cast<char const*>(&_running), sizeof(_running));
    file.write(reinterpret_cast<char const*>(&_jumps), sizeof(_jumps));
    file.write(reinterpret_cast<char const*>(&_flying), sizeof(_flying));
    file.write(reinterpret_cast<char const*>(&_cross_wall), sizeof(_cross_wall));
    file.write(reinterpret_cast<char const*>(&_held_item_stack_index), sizeof(_held_item_stack_index));
    file.write(reinterpret_cast<char const*>(&_health), sizeof(_health));
    file.write(reinterpret_cast<char const*>(&_game_mode), sizeof(_game_mode));
    file.write(reinterpret_cast<char const*>(&GameTime), sizeof(GameTime));
    file.write(reinterpret_cast<char const*>(_inventory.data()), sizeof(_inventory));
    return true;
}

auto Player::add_item(items::ItemStack stack) -> bool {
    constexpr auto MAX_STACK_COUNT = 255;
    for (auto& row: _inventory | std::views::reverse) {
        for (auto& slot: row) {
            if (slot.id == stack.id && slot.count < MAX_STACK_COUNT) {
                if (stack.count + slot.count <= MAX_STACK_COUNT) {
                    slot.count += stack.count;
                    return true;
                }
                stack.count -= MAX_STACK_COUNT - slot.count;
                slot.count = MAX_STACK_COUNT;
            }
        }
    }
    for (auto& row: _inventory | std::views::reverse) {
        for (auto& slot: row) {
            if (slot.empty()) {
                slot.id = stack.id;
                if (stack.count <= MAX_STACK_COUNT) {
                    slot.count = stack.count;
                    return true;
                }
                slot.count = MAX_STACK_COUNT;
                stack.count -= MAX_STACK_COUNT;
            }
        }
    }
    return false;
}
}

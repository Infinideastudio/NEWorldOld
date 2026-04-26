module;

#include <spdlog/spdlog.h>
#undef assert

export module commands;
import std;
import types;
import math;
import debug;
import worlds;

using std::vector;
using std::span;
using std::string;
using std::string_view;

template <typename T>
auto _parse_int(string_view s) -> T {
    return static_cast<T>(std::stoull(string(s)));
}

template <typename T>
auto _parse_float(string_view s) -> T {
    return static_cast<T>(std::stod(string(s)));
}

auto _split(string_view s, string_view separators) -> vector<string_view> {
    auto res = vector<string_view>();
    auto left = 0uz;
    while (true) {
        auto right = s.find_first_of(separators, left);
        if (right == s.npos) {
            right = s.length();
        }
        if (left < right) {
            res.push_back(s.substr(left, right - left));
        }
        if (right == s.length()) {
            break;
        }
        left = right + 1;
    }
    return res;
}

export class Command {
public:
    using Func = std::function<auto(span<string_view>, worlds::World&, vector<string>&)->bool>;

    template <typename F>
    Command(F f):
        func(std::move(f)){};

    Func func;
};

export class CommandRegistry {
public:
    void add(string id, Command command) {
        if (_entries.contains(id)) {
            unimplemented();
        }
        _entries.emplace(std::move(id), std::move(command));
    }

    auto entries() const -> std::unordered_map<string, Command> const& {
        return _entries;
    }

    auto execute_on(string_view s, worlds::World& world, vector<string>& messages) -> bool {
        auto parts = _split(s, " ");
        if (!parts.empty()) {
            auto it = _entries.find(string(parts[0]));
            if (it != _entries.end()) {
                if (it->second.func(parts, world, messages)) {
                    return true;
                }
            }
        }
        spdlog::warn("Fail to execute the command: {}", s);
        messages.emplace_back(std::format("Fail to execute the command: {}", s));
        return false;
    }

    auto try_auto_complete(string_view s) -> std::optional<string> {
        for (auto& [id, _]: _entries) {
            if (id.starts_with(s)) {
                return id;
            }
        }
        return {};
    }

private:
    std::unordered_map<string, Command> _entries;
};

export void register_base_commands(CommandRegistry& r) {
    r.add("/help", [](span<string_view> command, worlds::World&, vector<string>& messages) {
        if (command.size() != 1)
            return false;
        messages.emplace_back(
            "Controls: W/A/S/D/SPACE/SHIFT = move, R/F = fast move (creative mode), E = open inventory,"
        );
        messages.emplace_back("          left/right mouse button = break/place blocks, mouse wheel = select blocks,");
        messages.emplace_back("          F1 = switch game mode, F2 = take screenshot, F3 = switch debug panel,");
        messages.emplace_back("          F4 = switch cross wall (creative mode), F5 = switch HUD,");
        messages.emplace_back("          F7 = switch full screen mode, F8 = fast forward game time");
        messages.emplace_back(
            "Commands: /help | /clear | /kit | /give <id> <amount> | /tp <x> <y> <z> | /clearinventory | /suicide |"
        );
        messages.emplace_back(
            "          /setblock <x> <y> <z> <id> | /tree <x> <y> <z> | /explode <x> <y> <z> <radius> | /time <time>"
        );
        return true;
    });

    r.add("/clear", [](span<string_view> command, worlds::World&, vector<string>& messages) {
        if (command.size() != 1)
            return false;
        messages.clear();
        return true;
    });

    r.add("/kit", [](span<string_view> command, worlds::World& world, vector<string>& messages) {
        if (command.size() != 1)
            return false;
        auto& player = world.player();
        for (auto i = 0uz; i < blocks::block_info_registry.entries().size(); i++) {
            auto id = blocks::Id(i);
            player.add_item({id, 255});
        }
        return true;
    });

    r.add("/give", [](span<string_view> command, worlds::World& world, vector<string>& messages) {
        if (command.size() != 3)
            return false;
        auto itemid = _parse_int<blocks::Id>(command[1]);
        auto amount = _parse_int<size_t>(command[2]);
        world.player().add_item({itemid, amount});
        return true;
    });

    r.add("/tp", [](span<string_view> command, worlds::World& world, vector<string>& messages) {
        if (command.size() != 4)
            return false;
        auto x = _parse_float<double>(command[1]);
        auto y = _parse_float<double>(command[2]);
        auto z = _parse_float<double>(command[3]);
        world.player().set_coord({x, y, z});
        return true;
    });

    r.add("/clearinventory", [](span<string_view> command, worlds::World& world, vector<string>& messages) {
        if (command.size() != 1)
            return false;
        world.player().clear_inventory();
        return true;
    });

    r.add("/suicide", [](span<string_view> command, worlds::World& world, vector<string>& messages) {
        if (command.size() != 1)
            return false;
        world.player().spawn();
        return true;
    });

    r.add("/setblock", [](span<string_view> command, worlds::World& world, vector<string>& messages) {
        if (command.size() != 5)
            return false;
        auto x = _parse_int<int32_t>(command[1]);
        auto y = _parse_int<int32_t>(command[2]);
        auto z = _parse_int<int32_t>(command[3]);
        auto b = _parse_int<blocks::Id>(command[4]);
        world.put_block(Vec3i(x, y, z), b);
        return true;
    });

    r.add("/tree", [](span<string_view> command, worlds::World& world, vector<string>& messages) {
        if (command.size() != 4)
            return false;
        auto x = _parse_int<int32_t>(command[1]);
        auto y = _parse_int<int32_t>(command[2]);
        auto z = _parse_int<int32_t>(command[3]);
        world.build_tree(Vec3i(x, y, z));
        return true;
    });

    r.add("/explode", [](span<string_view> command, worlds::World& world, vector<string>& messages) {
        if (command.size() != 5)
            return false;
        auto x = _parse_int<int32_t>(command[1]);
        auto y = _parse_int<int32_t>(command[2]);
        auto z = _parse_int<int32_t>(command[3]);
        auto r = _parse_int<int32_t>(command[4]);
        world.explode(Vec3i(x, y, z), r);
        return true;
    });

    r.add("/time", [](span<string_view> command, worlds::World&, vector<string>& messages) {
        if (command.size() != 2)
            return false;
        auto time = _parse_int<int32_t>(command[1]);
        if (time < 0)
            return false;
        // TODO: make game time a property of world
        // GameTime = time;
        return true;
    });

    r.add("/gamemode", [](span<string_view> command, worlds::World& world, vector<string>& messages) {
        if (command.size() != 2)
            return false;
        auto mode = _parse_int<player::Player::GameMode>(command[1]);
        world.player().set_game_mode(mode);
        return true;
    });
}

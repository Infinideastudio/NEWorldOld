export module commands;
import std;
import types;
import worlds;

export class Command {
public:
    Command(std::string identifier, std::function<bool(std::vector<std::string> const&, worlds::World& world)> execute):
        identifier(std::move(identifier)),
        execute(std::move(execute)) {};

    std::string identifier;
    std::function<bool(std::vector<std::string> const&, worlds::World& world)> execute;
};

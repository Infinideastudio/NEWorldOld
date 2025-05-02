module;

#include <functional>
#include <string>
#include <utility>
#include <vector>

export module commands;
import worlds;

export class Command {
public:
    Command(std::string identifier, std::function<bool(std::vector<std::string> const&, World& world)> execute):
        identifier(std::move(identifier)),
        execute(std::move(execute)) {};

    std::string identifier;
    std::function<bool(std::vector<std::string> const&, World& world)> execute;
};

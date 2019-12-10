#pragma once

#include <utility>
#include <functional>

class Command {
public:
    Command(std::string _identifier, std::function<bool(const std::vector<std::string> &)> _execute) : identifier(
            std::move(std::move(_identifier))), execute(std::move(std::move(_execute))) {};

    std::string identifier;
    std::function<bool(const std::vector<std::string> &)> execute;
};

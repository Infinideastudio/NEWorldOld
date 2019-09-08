#pragma once
#include <utility>
#include <utility>
#include "StdInclude.h"

class Command {
public:
	Command(std::string _identifier, std::function<bool(const vector<std::string>&)> _execute) :identifier(std::move(std::move(_identifier))), execute(std::move(std::move(_execute))) {};

	std::string identifier;
	std::function<bool(const vector<std::string>&)> execute;
};

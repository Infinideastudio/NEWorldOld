#pragma once
#include "Definitions.h"

class Command {
public:
	Command(string _identifier, std::function<bool(const vector<string>&)> _execute) :identifier(_identifier), execute(_execute) {};

	string identifier;
	std::function<bool(const vector<string>&)> execute;
};

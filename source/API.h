#pragma once
#include <functional>
#include <string>
#include <map>
#include <vector>
#include "Definitions.h"
#include "World.h"
#include "Chunk.h"
#include "Command.h"
#include "ModSupport.h"
extern vector<Command> commands;

namespace API {

	std::map<std::string, void*> sharedData;

	struct APIPackage {
		std::function<World::chunk*(int cx, int cy, int cz)> getChunk;
		std::function<block(int cx, int cy, int cz)> getBlock;
		std::function<void(int x, int y, int z, block Block)> setBlock;
		std::function<Command*(string commandName)> getCommand;
		std::function<bool(Command command)> registerCommand;
		std::function<void*(std::string key)> getSharedData;
		std::function<void(std::string key, void* value)> setSharedData;
		std::function<PlayerData()> getPlayerData;
	};

	APIPackage getPackage();
}
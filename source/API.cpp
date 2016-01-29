#include "API.h"

API::APIPackage API::getPackage() {
	static APIPackage api;
	static bool init = false;
	if (init) return api;
	api.getChunk = World::getChunkPtr;
	api.getBlock = std::bind(World::getblock, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, 0, nullptr);
	api.setBlock = std::bind(World::setblock, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, nullptr);
	api.getCommand = [](string s) -> Command* {
		for (size_t i = 0; i < commands.size(); i++)
			if (commands[i].identifier == s) return &commands[i];
		return nullptr;
	};
	api.registerCommand = [](Command c) -> bool {
		for (size_t i = 0; i < commands.size(); i++)
			if (commands[i].identifier == c.identifier) return false;
		commands.push_back(c);
		return true;
	};
	api.getSharedData = [](std::string key) -> void* {
		std::map<std::string, void*>::iterator iter = sharedData.find(key);
		if (iter == sharedData.end()) return nullptr;
		return iter->second;
	};
	api.setSharedData = [](std::string key, void* value) {
		sharedData[key] = value;
	};
	init = true;
	return api;
}

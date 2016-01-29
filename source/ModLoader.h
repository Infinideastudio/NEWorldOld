#pragma once
#include <string>
#include <vector>
#include "ModLoader.h"

namespace Mod {

	class ModLoader {
	public:
		struct ModInfo {
			std::string name;
			std::string version;
			std::string dependence;
			void* call;
		};
		std::vector<ModInfo> mods;

		void loadMods();

	private:
		enum ModLoadStatus { Success, MissDependence, InitFailed };
		ModLoadStatus loadSingleMod(std::string modPath);

		typedef void* ModCall;
		static ModCall loadMod(std::string filename);
		static void* getFunction(ModCall call, std::string functionName);
		static void unloadMod(ModCall call);
	};

}
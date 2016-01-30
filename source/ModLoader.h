#pragma once
#include <string>
#include <vector>
#include "ModLoader.h"
#include "ModSupport.h"

namespace Mod {

	class ModLoader {
	public:
		static std::vector<ModInfo> mods;
		static void loadMods();

	private:
		enum ModLoadStatus { Success, MissDependence, InitFailed };
		static ModLoadStatus loadSingleMod(std::string modPath);

		typedef void* ModCall;
		typedef FARPROC FuncPtr;
		static ModCall loadMod(std::string filename);
		static void unloadMods();
		static FuncPtr getFunction(ModCall call, std::string functionName);
		static void unloadMod(ModCall call);
	};

}
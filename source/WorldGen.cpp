#include "Definitions.h"
#include "WorldGen.h"

namespace WorldGen{
	int		seed;
	int		WaterLevel = 30;
	wxString curWorldGen;
	GetHeightFunc heightfunc;
	void Init(int mapseed){
		seed = mapseed;
		wxTextFile config(L"worldgen");
		config.Open();
		curWorldGen = config.GetFirstLine();
		wxDynamicLibrary* lib;
		lib = new wxDynamicLibrary(curWorldGen);
		if (lib->HasSymbol(L"Init"))
			((InitFunc)lib->GetSymbol(L"Init"))(seed);
		heightfunc = (GetHeightFunc)lib->GetSymbol(L"GetHeight");
	}
	int getHeight(int x, int y)
	{
#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
		c_getHeightFromWorldGen++;
#endif
		return heightfunc(x, y);
	}
}
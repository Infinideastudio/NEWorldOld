#include "Definitions.h"
#include "WorldGen.h"

namespace WorldGen{
	int		seed;
	int		WaterLevel = 30;
	wxString curWorldGen;
	GetHeightFunc heightfunc;
	void Init(int mapseed){
		seed = mapseed;
		wxDynamicLibrary* lib = new wxDynamicLibrary(CurrentWorldGen);
		if (lib->HasSymbol(L"Init"))
			((InitFunc)lib->GetSymbol(L"Init"))(seed);
		heightfunc = (GetHeightFunc)lib->GetSymbol(L"GetHeight");
		lib->Detach();
		delete lib;
	}
	int getHeight(int x, int y)
	{
#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
		c_getHeightFromWorldGen++;
#endif
		return heightfunc(x, y);
	}
}
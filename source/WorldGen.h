#ifndef WORLDGEN_H
#define WORLDGEN_H
#include "Definitions.h"

namespace WorldGen{
	typedef void(* InitFunc)(int seed);
	typedef int(* GetHeightFunc)(int x, int y);
	extern int WaterLevel;
	void Init(int mapseed);
	int getHeight(int x, int y);

}
#endif
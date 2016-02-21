#pragma once
#include "Definitions.h"

//Perlin Noise 2D
namespace WorldGen{

	extern double perm[256];
	extern int seed;
	extern double NoiseScaleX;
	extern double NoiseScaleZ;
	extern int WaterLevel;

	void perlinNoiseInit(int mapseed);
	inline double Noise(int x, int y){
		/*这个算法被我玩坏了QAQ
		UINT c = seed * 65536 + x*256+y+2;
		double a = ((((((c*c*c) % 259)*c*c) % 259 * c*c) % 259 - 2) % 32768) / 256.0;
		return a;*/
		//这个与时间相关的还是不行
		/*double a = ((GetTickCount64() + seed*x * 8 + seed*y) % 32768)/256.0;
		return a;*/
		//强势混搭
		long long xx = (x*seed^y)*seed;
		/*UINT c = seed * y + x + 2;
		double a = (((((c*c*c) % 259)*c*c) % 259 * c*c) % 259 - 2) / 16777216.0;
		a += ((xx*(xx*xx * 15731 + 789221) + 1376312589) & 0x7fffffff) / 16777216.0;*/
		return  ((xx*(xx*xx * 15731 + 789221) + 1376312589) & 0x7fffffff) / 16777216.0;;
	}
	double SmoothedNoise(int x, int y);
	inline double Interpolate(double a, double b, double x) { return a*(1.0 - x) + b*x; }
	double InterpolatedNoise(double x, double y);
	double PerlinNoise2D(double x, double y);
	inline int getHeight(int x, int y){
#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
		c_getHeightFromWorldGen++;
#endif
		return (int)PerlinNoise2D(x / NoiseScaleX + 0.125 , y / NoiseScaleZ + 0.125) >> 2;
	}

}
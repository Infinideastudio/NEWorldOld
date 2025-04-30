#pragma once
#include "Definitions.h"

//Perlin Noise 2D
namespace WorldGen {

    extern double perm[256];
    extern int seed;
    extern double NoiseScaleX;
    extern double NoiseScaleZ;
    extern int WaterLevel;

    void perlinNoiseInit(int mapseed);
    inline double Noise(int x, int y) {
        long long xx = x + y * 13258953287;
        xx = (xx >> 13) ^ xx;
        return ((xx*(xx*xx * 15731 + 789221) + 1376312589)& 0x7fffffff) / 16777216.0;
    }
    double SmoothedNoise(int x, int y);
    inline double Interpolate(double a, double b, double x) { return a*(1.0 - x) + b*x; }
    double InterpolatedNoise(double x, double y);
    double PerlinNoise2D(double x, double y);
    inline int getHeight(int x, int y) {
#ifdef NEWORLD_DEBUG_PERFORMANCE_REC
        c_getHeightFromWorldGen++;
#endif
        int mountain = int(PerlinNoise2D(x / NoiseScaleX / 2.0 + 34.0, y / NoiseScaleZ / 2.0 + 4.0));
        int upper = (int(PerlinNoise2D(x / NoiseScaleX + 0.125, y / NoiseScaleZ + 0.125)) >> 3) + 96;
        int transition = int(PerlinNoise2D(x / NoiseScaleX + 34.0, y / NoiseScaleZ + 4.0));
        int lower = (int(PerlinNoise2D(x / NoiseScaleX + 0.125, y / NoiseScaleZ + 0.125)) >> 3);
        int base = int(PerlinNoise2D(x / NoiseScaleX / 16.0, y / NoiseScaleZ / 16.0)) * 2 - 320;
        if(transition > upper) {
            if (mountain > upper) return mountain + base;
            return upper + base;
        }
        if (transition < lower) return lower + base;
        return transition + base;
        //return int(PerlinNoise2D(x / NoiseScaleX, y / NoiseScaleZ) / 4 - 80 + WaterLevel);
    }

}
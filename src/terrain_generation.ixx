module;

#include <array>
#include <cmath>

export module terrain_generation;
import globals;

export constexpr int WaterLevel = 96; // Water level
export constexpr double NoiseScaleX = 64;
export constexpr double NoiseScaleZ = 64;

// Perlin Noise 2D
export namespace WorldGen {

std::array<double, 256> perm = {};
int seed = 0;

void perlinNoiseInit(int mapseed) {
    fastSrand(mapseed);
    for (int i = 0; i < 256; i++)
        perm[i] = rnd() * 256.0;
    seed = mapseed;
}

auto Noise(int x, int y) -> double {
    long long xx = x + y * 13258953287;
    xx = (xx >> 13) ^ xx;
    return ((xx * (xx * xx * 15731 + 789221) + 1376312589) & 0x7fffffff) / 16777216.0;
}

auto SmoothedNoise(int x, int y) -> double {
    double corners, sides, center;
    corners = (Noise(x - 1, y - 1) + Noise(x + 1, y - 1) + Noise(x - 1, y + 1) + Noise(x + 1, y + 1)) / 8.0;
    sides = (Noise(x - 1, y) + Noise(x + 1, y) + Noise(x, y - 1) + Noise(x, y + 1)) / 4.0;
    center = Noise(x, y);
    return corners + sides + center;
}

auto Interpolate(double a, double b, double x) -> double {
    return a * (1.0 - x) + b * x;
}

auto InterpolatedNoise(double x, double y) -> double {
    int int_X = static_cast<int>(std::floor(x));
    double fractional_X = x - int_X;
    int int_Y = static_cast<int>(std::floor(y));
    double fractional_Y = y - int_Y;
    double v1 = Noise(int_X, int_Y);
    double v2 = Noise(int_X + 1, int_Y);
    double v3 = Noise(int_X, int_Y + 1);
    double v4 = Noise(int_X + 1, int_Y + 1);
    double i1 = Interpolate(v1, v2, fractional_X);
    double i2 = Interpolate(v3, v4, fractional_X);
    return Interpolate(i1, i2, fractional_Y);
}

auto PerlinNoise2D(double x, double y) -> double {
    double total = 0, frequency = 1, amplitude = 1;
    for (int i = 0; i <= 4; i++) {
        total += InterpolatedNoise(x * frequency, y * frequency) * amplitude;
        frequency *= 2;
        amplitude /= 2.0;
    }
    return total;
}

auto getHeight(int x, int y) -> int {
    int mountain = int(PerlinNoise2D(x / NoiseScaleX / 2.0 + 34.0, y / NoiseScaleZ / 2.0 + 4.0));
    int upper = (int(PerlinNoise2D(x / NoiseScaleX + 0.125, y / NoiseScaleZ + 0.125)) >> 3) + 96;
    int transition = int(PerlinNoise2D(x / NoiseScaleX + 34.0, y / NoiseScaleZ + 4.0));
    int lower = (int(PerlinNoise2D(x / NoiseScaleX + 0.125, y / NoiseScaleZ + 0.125)) >> 3);
    int base = int(PerlinNoise2D(x / NoiseScaleX / 16.0, y / NoiseScaleZ / 16.0)) * 2 - 320;
    if (transition > upper) {
        if (mountain > upper)
            return mountain + base;
        return upper + base;
    }
    if (transition < lower)
        return lower + base;
    return transition + base;
    // return int(PerlinNoise2D(x / NoiseScaleX, y / NoiseScaleZ) / 4 - 80 + WaterLevel);
}
}

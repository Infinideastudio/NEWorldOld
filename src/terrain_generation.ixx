export module terrain_generation;
import std;
import types;
import globals;

// Fractal noise 2D
export namespace WorldGen {

constexpr int WaterLevel = 96; // Water level
constexpr double NoiseScaleX = 64;
constexpr double NoiseScaleZ = 64;

std::array<double, 256> perm = {};
int seed = 0;

void noiseInit(int mapseed) {
    fastSrand(mapseed);
    for (int i = 0; i < 256; i++)
        perm[i] = rnd() * 256.0;
    seed = mapseed;
}

auto noise(int x, int y) -> double {
    auto xx = x + y * 13258953287LL;
    xx = (xx >> 13) ^ xx;
    return static_cast<double>((xx * (xx * xx * 15731 + 789221) + 1376312589) & 0x7fffffff) / 16777216.0;
}

auto smoothedNoise(int x, int y) -> double {
    auto corners = (noise(x - 1, y - 1) + noise(x + 1, y - 1) + noise(x - 1, y + 1) + noise(x + 1, y + 1)) / 8.0;
    auto sides = (noise(x - 1, y) + noise(x + 1, y) + noise(x, y - 1) + noise(x, y + 1)) / 4.0;
    auto center = noise(x, y);
    return corners + sides + center;
}

auto interpolate(double a, double b, double x) -> double {
    return a * (1.0 - x) + b * x;
}

auto interpolatedNoise(double x, double y) -> double {
    int int_X = static_cast<int>(std::floor(x));
    double fractional_X = x - int_X;
    int int_Y = static_cast<int>(std::floor(y));
    double fractional_Y = y - int_Y;
    double v1 = noise(int_X, int_Y);
    double v2 = noise(int_X + 1, int_Y);
    double v3 = noise(int_X, int_Y + 1);
    double v4 = noise(int_X + 1, int_Y + 1);
    double i1 = interpolate(v1, v2, fractional_X);
    double i2 = interpolate(v3, v4, fractional_X);
    return interpolate(i1, i2, fractional_Y);
}

auto fractalNoise2D(double x, double y) -> double {
    double total = 0, frequency = 1, amplitude = 1;
    for (int i = 0; i <= 4; i++) {
        total += interpolatedNoise(x * frequency, y * frequency) * amplitude;
        frequency *= 2;
        amplitude /= 2.0;
    }
    return total;
}

auto getHeight(int x, int y) -> int {
    auto mountain = int(fractalNoise2D(x / NoiseScaleX / 2.0 + 34.0, y / NoiseScaleZ / 2.0 + 4.0));
    auto upper = (int(fractalNoise2D(x / NoiseScaleX + 0.125, y / NoiseScaleZ + 0.125)) >> 3) + 96;
    auto transition = int(fractalNoise2D(x / NoiseScaleX + 34.0, y / NoiseScaleZ + 4.0));
    auto lower = (int(fractalNoise2D(x / NoiseScaleX + 0.125, y / NoiseScaleZ + 0.125)) >> 3);
    auto base = int(fractalNoise2D(x / NoiseScaleX / 16.0, y / NoiseScaleZ / 16.0)) * 2 - 320;
    if (transition > upper) {
        if (mountain > upper)
            return mountain + base;
        return upper + base;
    }
    if (transition < lower)
        return lower + base;
    return transition + base;
    // return int(fractalNoise2D(x / NoiseScaleX, y / NoiseScaleZ) / 4 - 80 + WaterLevel);
}
}

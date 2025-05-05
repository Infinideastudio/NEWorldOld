export module terrain_generation;
import std;
import types;
import globals;

// Fractal noise 2D
namespace terrain_generation {

export constexpr auto WATER_LEVEL = int32_t{96}; // Water level
constexpr auto NOISE_SCALE_X = 64.0;
constexpr auto NOISE_SCALE_Z = 64.0;

auto _perm = std::array<double, 256>{};
auto _seed = uint32_t{0};

export void noise_init(uint32_t seed) {
    fast_srand(seed);
    for (auto& i: _perm)
        i = rnd() * 256.0;
    _seed = seed;
}

auto noise_2d(int32_t x, int32_t y) -> double {
    auto xx = x + y * uint64_t{13258953287};
    xx = (xx >> 13) ^ xx;
    return static_cast<double>((xx * (xx * xx * 15731 + 789221) + 1376312589) & 0x7fffffff) / 16777216.0;
}

auto interpolated_noise_2d(double x, double y) -> double {
    auto int_x = static_cast<int32_t>(std::floor(x));
    auto fract_x = x - int_x;
    auto int_y = static_cast<int32_t>(std::floor(y));
    auto fract_y = y - int_y;
    auto v0 = noise_2d(int_x, int_y);
    auto v1 = noise_2d(int_x + 1, int_y);
    auto v2 = noise_2d(int_x, int_y + 1);
    auto v3 = noise_2d(int_x + 1, int_y + 1);
    return std::lerp(std::lerp(v0, v1, fract_x), std::lerp(v2, v3, fract_x), fract_y);
}

auto fractal_noise_2d(double x, double y) -> double {
    auto total = 0.0, frequency = 1.0, amplitude = 1.0;
    for (auto i = 0; i <= 4; i++) {
        total += interpolated_noise_2d(x * frequency, y * frequency) * amplitude;
        frequency *= 2;
        amplitude /= 2.0;
    }
    return total;
}

export auto get_height(int32_t x, int32_t y) -> int32_t {
    auto mountain =
        static_cast<int32_t>(fractal_noise_2d(x / NOISE_SCALE_X / 2.0 + 34.0, y / NOISE_SCALE_Z / 2.0 + 4.0));
    auto upper = static_cast<int32_t>(fractal_noise_2d(x / NOISE_SCALE_X + 0.125, y / NOISE_SCALE_Z + 0.125)) / 8 + 96;
    auto transition = static_cast<int32_t>(fractal_noise_2d(x / NOISE_SCALE_X + 34.0, y / NOISE_SCALE_Z + 4.0));
    auto lower = static_cast<int32_t>(fractal_noise_2d(x / NOISE_SCALE_X + 0.125, y / NOISE_SCALE_Z + 0.125)) / 8;
    auto base = static_cast<int32_t>(fractal_noise_2d(x / NOISE_SCALE_X / 16.0, y / NOISE_SCALE_Z / 16.0)) * 2 - 320;
    if (transition > upper) {
        if (mountain > upper) {
            return mountain + base;
        }
        return upper + base;
    }
    if (transition < lower) {
        return lower + base;
    }
    return transition + base;
}
}

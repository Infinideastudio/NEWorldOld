export module math:aabb;
import std;
import types;
import :vector;

// Axis-aligned bounding box.
export template <typename T, size_t N, T EPS>
class AABB {
public:
    Vector<T, N> min;
    Vector<T, N> max;

    // Lower and upper bounds constructor.
    constexpr AABB(Vector<T, N> min, Vector<T, N> max):
        min(min),
        max(max) {}

    // Type conversion constructor.
    template <typename U, U EPS_>
    constexpr explicit AABB(AABB<U, N, EPS_> r):
        min(static_cast<Vector<T, N>>(r.min)),
        max(static_cast<Vector<T, N>>(r.max)) {}

    // Returns if `this` intersects `other` on the i-th axis.
    auto intersects_on_axis(size_t i, AABB const& other, T eps = T(0)) const -> bool {
        auto a = min[i];
        auto b = max[i];
        auto c = other.min[i] + eps;
        auto d = other.max[i] - eps;
        return (a > c && a < d) || (b > c && b < d) || (c > a && c < b) || (d > a && d < b);
    }

    // Returns if `this` intersects `other`.
    auto intersects(AABB const& other, T eps = T(0)) const -> bool {
        for (auto i = 0uz; i < N; i++) {
            if (!intersects_on_axis(i, other, eps)) {
                return false;
            }
        }
        return true;
    }

    // Returns the clipped displacement on the i-th axis, by testing against `other`.
    auto clip_displacement_on_axis(size_t i, AABB const& other, T disp, T eps = EPS) const -> T {
        for (auto j = 0uz; j < N; j++) {
            if (j != i && !intersects_on_axis(j, other, eps)) {
                return disp;
            }
        }
        if (disp < T(0) && max[i] > other.min[i]) {
            return std::min(T(0), -std::min(min[i] - other.max[i], -disp));
        }
        if (disp > T(0) && min[i] < other.max[i]) {
            return std::max(T(0), std::min(other.min[i] - max[i], disp));
        }
        return disp;
    }

    // Returns the clipped displacement on the i-th axis, by testing against `others`.
    auto clip_displacement_on_axis(size_t i, std::vector<AABB> const& others, T disp, T eps = EPS) const -> T {
        for (auto const& other: others) {
            disp = clip_displacement_on_axis(i, other, disp, eps);
        }
        return disp;
    }

    // Returns the clipped displacement, by testing against `others`.
    auto clip_displacement(std::vector<AABB> const& others, Vector<T, N> disp, T eps = EPS) const -> Vector<T, N> {
        auto t = *this;
        for (auto i = 0uz; i < N; i++) {
            disp[i] = t.clip_displacement_on_axis(i, others, disp[i], eps);
            t.min[i] += disp[i];
            t.max[i] += disp[i];
        }
        return disp;
    }

    // Returns the AABB displaced by the given vector.
    auto operator+(Vector<T, N> const& disp) const -> AABB {
        return {min + disp, max + disp};
    };

    auto operator-(Vector<T, N> const& disp) const -> AABB {
        return {min - disp, max - disp};
    };

    auto operator+=(Vector<T, N> const& r) -> AABB& {
        return *this = *this + r;
    }

    auto operator-=(Vector<T, N> const& r) -> AABB& {
        return *this = *this - r;
    }

    // Returns the AABB extended by the given vector.
    auto extend(Vector<T, N> const& extent) -> AABB const {
        return {
            min + extent.map([](auto x) { return std::min(x, 0.0); }),
            max + extent.map([](auto x) { return std::max(x, 0.0); }),
        };
    }
};

export template <typename T, T EPS>
using AABB3 = AABB<T, 3, EPS>;

export using AABB3f = AABB3<float, 1e-4f>;
export using AABB3d = AABB3<double, 1e-8>;

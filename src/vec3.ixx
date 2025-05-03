module;

#include <cmath>
#include <utility>

export module vec3;

export template <typename T>
class Vec3 {
public:
    T x = {};
    T y = {};
    T z = {};

    // Zero vector constructor
    Vec3() = default;

    // Same-value constructor
    Vec3(T x):
        x(x),
        y(x),
        z(x) {}

    // Element-wise vector constructor
    Vec3(T x, T y, T z):
        x(x),
        y(y),
        z(z) {}

    // Type conversion constructor
    template <typename U>
    explicit Vec3(Vec3<U> const& r):
        x(static_cast<T>(r.x)),
        y(static_cast<T>(r.y)),
        z(static_cast<T>(r.z)) {}

    // Element-wise equality
    auto operator==(Vec3 const& r) const -> bool {
        return x == r.x && y == r.y && z == r.z;
    }

    // Element-wise inequality
    auto operator!=(Vec3 const& r) const -> bool {
        return !(*this == r);
    }

    // Lexicographical ordering
    auto operator<(Vec3 const& r) const -> bool {
        if (x != r.x)
            return x < r.x;
        if (y != r.y)
            return y < r.y;
        if (z != r.z)
            return z < r.z;
        return false;
    }

    friend auto operator-(Vec3 const& v) -> Vec3 {
        return Vec3(-v.x, -v.y, -v.z);
    }

    auto operator+(Vec3 const& r) const -> Vec3 {
        return Vec3(x + r.x, y + r.y, z + r.z);
    };

    auto operator-(Vec3 const& r) const -> Vec3 {
        return Vec3(x - r.x, y - r.y, z - r.z);
    };

    auto operator*(Vec3 const& r) const -> Vec3 {
        return Vec3(x * r.x, y * r.y, z * r.z);
    };

    auto operator/(Vec3 const& r) const -> Vec3 {
        return Vec3(x / r.x, y / r.y, z / r.z);
    };

    auto operator*(T value) const -> Vec3 {
        return Vec3(x * value, y * value, z * value);
    }

    auto operator/(T value) const -> Vec3 {
        return Vec3(x / value, y / value, z / value);
    }

    auto operator+=(Vec3 const& r) -> Vec3& {
        return *this = *this + r;
    }

    auto operator-=(Vec3 const& r) -> Vec3& {
        return *this = *this - r;
    }

    auto operator*=(Vec3 const& r) -> Vec3& {
        return *this = *this * r;
    }

    auto operator/=(Vec3 const& r) -> Vec3& {
        return *this = *this / r;
    }

    auto operator*=(T value) -> Vec3& {
        return *this = *this * value;
    }

    auto operator/=(T value) -> Vec3& {
        return *this = *this / value;
    }

    // Element-wise function application
    template <typename F>
    auto map(F f) const -> Vec3 {
        return Vec3(f(x), f(y), f(z));
    }

    // Swaps two vectors
    friend void swap(Vec3& l, Vec3& r) noexcept {
        using std::swap;
        swap(l.x, r.x);
        swap(l.y, r.y);
        swap(l.z, r.z);
    }

    // Returns the square of vector length
    auto length_sqr() const -> T {
        return x * x + y * y + z * z;
    }

    // Returns vector length
    auto length() const -> T {
        return std::sqrt(length_sqr());
    }

    // Normalize vector
    auto normalize() const -> Vec3 {
        return *this / length();
    }
};

export using Vec3i = Vec3<int>;
export using Vec3u = Vec3<unsigned int>;
export using Vec3f = Vec3<float>;
export using Vec3d = Vec3<double>;

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

    template <typename U>
    Vec3(Vec3<U> const& rhs):
        x(static_cast<T>(rhs.x)),
        y(static_cast<T>(rhs.y)),
        z(static_cast<T>(rhs.z)) {}

    // Returns the square of vector length
    auto lengthSqr() const -> T {
        return x * x + y * y + z * z;
    }

    // Returns vector length
    auto length() const -> T {
        return std::sqrt(lengthSqr());
    }

    // Normalize vector
    auto normalize() const -> Vec3 {
        return *this / length();
    }

    // Element-wise equality
    auto operator==(Vec3 const& rhs) const -> bool {
        return x == rhs.x && y == rhs.y && z == rhs.z;
    }

    // Element-wise inequality
    auto operator!=(Vec3 const& rhs) const -> bool {
        return !(rhs == *this);
    }

    // Lexicographical ordering
    auto operator<(Vec3 const& rhs) const -> bool {
        if (x != rhs.x)
            return x < rhs.x;
        if (y != rhs.y)
            return y < rhs.y;
        if (z != rhs.z)
            return z < rhs.z;
        return false;
    }

    friend auto operator-(Vec3 const& vec) -> Vec3 {
        return Vec3(-vec.x, -vec.y, -vec.z);
    }

    auto operator+(Vec3 const& rhs) const -> Vec3 {
        return Vec3(x + rhs.x, y + rhs.y, z + rhs.z);
    };

    auto operator-(Vec3 const& rhs) const -> Vec3 {
        return Vec3(x - rhs.x, y - rhs.y, z - rhs.z);
    };

    auto operator*(Vec3 const& rhs) const -> Vec3 {
        return Vec3(x * rhs.x, y * rhs.y, z * rhs.z);
    };

    auto operator/(Vec3 const& rhs) const -> Vec3 {
        return Vec3(x / rhs.x, y / rhs.y, z / rhs.z);
    };

    auto operator*(T value) const -> Vec3 {
        return Vec3(x * value, y * value, z * value);
    }

    auto operator/(T value) const -> Vec3 {
        return Vec3(x / value, y / value, z / value);
    }

    auto operator+=(Vec3 const& rhs) -> Vec3& {
        return *this = *this + rhs;
    }

    auto operator-=(Vec3 const& rhs) -> Vec3& {
        return *this = *this - rhs;
    }

    auto operator*=(Vec3 const& rhs) -> Vec3& {
        return *this = *this * rhs;
    }

    auto operator/=(Vec3 const& rhs) -> Vec3& {
        return *this = *this / rhs;
    }

    auto operator*=(T value) -> Vec3& {
        return *this = *this * value;
    }

    auto operator/=(T value) -> Vec3& {
        return *this = *this / value;
    }

    template <typename Func>
    auto map(Func func) const -> Vec3 {
        return Vec3(func(x), func(y), func(z));
    }

    friend void swap(Vec3& lhs, Vec3& rhs) noexcept {
        using std::swap;
        swap(lhs.x, rhs.x);
        swap(lhs.y, rhs.y);
        swap(lhs.z, rhs.z);
    }
};

export using Vec3i = Vec3<int>;
export using Vec3f = Vec3<float>;
export using Vec3d = Vec3<double>;

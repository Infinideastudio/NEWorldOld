export module math:euler;
import std;
import types;
import :vector;
import :matrix;

// 3D Euler angles.
export template <typename T>
class Euler {
public:
    std::array<T, 3> angles;

    // Element-wise constructor.
    constexpr Euler(T heading = T(0), T pitch = T(0), T roll = T(0)):
        angles{heading, pitch, roll} {}

    // Type conversion constructor.
    template <typename U>
    constexpr explicit Euler(Euler<U> r):
        angles{static_cast<T>(r.angles[0]), static_cast<T>(r.angles[1]), static_cast<T>(r.angles[2])} {}

    auto heading() const -> T {
        return angles[0];
    }
    auto heading() -> T& {
        return angles[0];
    }
    auto pitch() const -> T {
        return angles[1];
    }
    auto pitch() -> T& {
        return angles[1];
    }
    auto roll() const -> T {
        return angles[2];
    }
    auto roll() -> T& {
        return angles[2];
    }

    // Lexicographical ordering.
    auto operator<=>(Euler const&) const = default;

    // Returns the same value with angles truncated to the range [-pi, pi].
    auto normalize() const -> Euler {
        constexpr auto PI_2 = std::numbers::pi_v<T> * T(2);
        return {
            angles[0] - std::round(angles[0] / PI_2) * PI_2,
            angles[1] - std::round(angles[1] / PI_2) * PI_2,
            angles[2] - std::round(angles[2] / PI_2) * PI_2,
        };
    }

    // Returns the direction vector corresponding to the Euler angles.
    auto direction() const -> Vec3<T> {
        auto sh = std::sin(angles[0]), ch = std::cos(angles[0]);
        auto sp = std::sin(angles[1]), cp = std::cos(angles[1]);
        return {-sh * cp, sp, -ch * cp};
    }

    // Returns the rotation matrix corresponding to the Euler angles.
    auto matrix() const -> Mat4<T> {
        auto res = Mat4(T(1));
        res = Mat4<T>::rotate(angles[2], Vec3<T>(T(0), T(0), T(1))) * res;
        res = Mat4<T>::rotate(angles[1], Vec3<T>(T(1), T(0), T(0))) * res;
        res = Mat4<T>::rotate(angles[0], Vec3<T>(T(0), T(1), T(0))) * res;
        return res;
    }

    // Returns the camera view matrix corresponding to the Euler angles.
    auto view_matrix() const -> Mat4<T> {
        return matrix().transpose(); // Inverse = transpose for rotation matrices.
    }
};

export using Eulerf = Euler<float>;
export using Eulerd = Euler<double>;

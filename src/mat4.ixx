module;

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>

export module mat4;
import vec3;

export template <typename T>
class Mat4 {
public:
    std::array<T, 4 * 4> data = {};

    // Zero matrix constructor
    Mat4() = default;

    // Identity matrix constructor
    Mat4(T x) {
        data[0] = data[5] = data[10] = data[15] = x;
    }

    // Returns pointer to the i-th row
    auto operator[](std::size_t i) -> T* {
        assert(index < 4);
        return data + i * 4;
    }

    // Returns const pointer to the i-th row
    auto operator[](std::size_t i) const -> T const* {
        assert(index < 4);
        return data + i * 4;
    }

    // Element-wise equality
    auto operator==(Mat4 const& r) const -> bool {
        for (auto i = 0uz; i < data.size(); ++i) {
            if (data[i] != r.data[i])
                return false;
        }
        return true;
    }

    // Element-wise inequality
    auto operator!=(Mat4 const& r) const -> bool {
        return !(*this == r);
    }

    friend auto operator-(Mat4 const& m) -> Mat4 {
        auto res = Mat4();
        for (auto i = 0uz; i < m.data.size(); ++i)
            res.data[i] = -m.data[i];
        return res;
    }

    auto operator+(Mat4 const& r) const -> Mat4 {
        auto res = Mat4();
        for (auto i = 0uz; i < data.size(); ++i)
            res.data[i] = data[i] + r.data[i];
        return res;
    }

    auto operator-(Mat4 const& r) const -> Mat4 {
        auto res = Mat4();
        for (auto i = 0uz; i < data.size(); ++i)
            res.data[i] = data[i] - r.data[i];
        return res;
    }

    auto operator*(Mat4 const& r) const -> Mat4 {
        auto res = Mat4();
        res.data[0] = data[0] * r.data[0] + data[1] * r.data[4] + data[2] * r.data[8] + data[3] * r.data[12];
        res.data[1] = data[0] * r.data[1] + data[1] * r.data[5] + data[2] * r.data[9] + data[3] * r.data[13];
        res.data[2] = data[0] * r.data[2] + data[1] * r.data[6] + data[2] * r.data[10] + data[3] * r.data[14];
        res.data[3] = data[0] * r.data[3] + data[1] * r.data[7] + data[2] * r.data[11] + data[3] * r.data[15];
        res.data[4] = data[4] * r.data[0] + data[5] * r.data[4] + data[6] * r.data[8] + data[7] * r.data[12];
        res.data[5] = data[4] * r.data[1] + data[5] * r.data[5] + data[6] * r.data[9] + data[7] * r.data[13];
        res.data[6] = data[4] * r.data[2] + data[5] * r.data[6] + data[6] * r.data[10] + data[7] * r.data[14];
        res.data[7] = data[4] * r.data[3] + data[5] * r.data[7] + data[6] * r.data[11] + data[7] * r.data[15];
        res.data[8] = data[8] * r.data[0] + data[9] * r.data[4] + data[10] * r.data[8] + data[11] * r.data[12];
        res.data[9] = data[8] * r.data[1] + data[9] * r.data[5] + data[10] * r.data[9] + data[11] * r.data[13];
        res.data[10] = data[8] * r.data[2] + data[9] * r.data[6] + data[10] * r.data[10] + data[11] * r.data[14];
        res.data[11] = data[8] * r.data[3] + data[9] * r.data[7] + data[10] * r.data[11] + data[11] * r.data[15];
        res.data[12] = data[12] * r.data[0] + data[13] * r.data[4] + data[14] * r.data[8] + data[15] * r.data[12];
        res.data[13] = data[12] * r.data[1] + data[13] * r.data[5] + data[14] * r.data[9] + data[15] * r.data[13];
        res.data[14] = data[12] * r.data[2] + data[13] * r.data[6] + data[14] * r.data[10] + data[15] * r.data[14];
        res.data[15] = data[12] * r.data[3] + data[13] * r.data[7] + data[14] * r.data[11] + data[15] * r.data[15];
        return res;
    }

    auto operator*(T value) const -> Mat4 {
        auto res = Mat4();
        for (auto i = 0uz; i < data.size(); ++i)
            res.data[i] = data[i] * value;
        return res;
    }

    auto operator/(T value) const -> Mat4 {
        auto res = Mat4();
        for (auto i = 0uz; i < data.size(); ++i)
            res.data[i] = data[i] / value;
        return res;
    }

    auto operator+=(Mat4 const& r) -> Mat4& {
        return *this = *this + r;
    }

    auto operator-=(Mat4 const& r) -> Mat4& {
        return *this = *this - r;
    }

    auto operator*=(Mat4 const& r) -> Mat4& {
        return *this = *this * r;
    }

    auto operator*=(T value) -> Mat4& {
        return *this = *this * value;
    }

    auto operator/=(T value) -> Mat4& {
        return *this = *this / value;
    }

    // Get transposed matrix
    auto transpose() const -> Mat4 {
        auto res = Mat4();
        res.data[0] = data[0], res.data[1] = data[4], res.data[2] = data[8], res.data[3] = data[12];
        res.data[4] = data[1], res.data[5] = data[5], res.data[6] = data[9], res.data[7] = data[13];
        res.data[8] = data[2], res.data[9] = data[6], res.data[10] = data[10], res.data[11] = data[14];
        res.data[12] = data[3], res.data[13] = data[7], res.data[14] = data[11], res.data[15] = data[15];
        return res;
    }

    // Swap row r1, row r2
    void swap_rows(std::size_t i0, std::size_t i1) {
        assert(r1 < 4 && r2 < 4);
        std::swap(data[i0 * 4 + 0], data[i1 * 4 + 0]);
        std::swap(data[i0 * 4 + 1], data[i1 * 4 + 1]);
        std::swap(data[i0 * 4 + 2], data[i1 * 4 + 2]);
        std::swap(data[i0 * 4 + 3], data[i1 * 4 + 3]);
    }

    // Row r *= k
    void mult_row(std::size_t i, T value) {
        assert(r < 4);
        data[i * 4 + 0] *= value;
        data[i * 4 + 1] *= value;
        data[i * 4 + 2] *= value;
        data[i * 4 + 3] *= value;
    }

    // Row dst += row src * k
    void mult_and_add(std::size_t j, std::size_t i, T value) {
        assert(i < 4 && j < 4);
        data[i * 4 + 0] += data[j * 4 + 0] * value;
        data[i * 4 + 1] += data[j * 4 + 1] * value;
        data[i * 4 + 2] += data[j * 4 + 2] * value;
        data[i * 4 + 3] += data[j * 4 + 3] * value;
    }

    // Get inverse matrix
    auto inverse() const -> Mat4 {
        auto res = Mat4(T(1));
        for (int i = 0; i < 4; i++) {
            int p = i;
            for (int j = i + 1; j < 4; j++)
                if (abs(data[j * 4 + i]) > abs(data[p * 4 + i]))
                    p = j;
            res.swap_rows(i, p);
            swap_rows(i, p);
            res.mult_row(i, T(1) / data[i * 4 + i]);
            mult_row(i, T(1) / data[i * 4 + i]);
            for (int j = i + 1; j < 4; j++) {
                res.mult_and_add(i, j, -data[j * 4 + i]);
                mult_and_add(i, j, -data[j * 4 + i]);
            }
        }
        for (int i = 3; i >= 0; i--) {
            for (int j = 0; j < i; j++) {
                res.mult_and_add(i, j, -data[j * 4 + i]);
                mult_and_add(i, j, -data[j * 4 + i]);
            }
        }
        return res;
    }

    // Element-wise function application
    template <typename F>
    auto map(F f) const -> Mat4 {
        auto res = Mat4();
        for (auto i = 0uz; i < data.size(); ++i)
            res.data[i] = f(data[i]);
        return res;
    }

    // Swaps two matrices
    friend void swap(Mat4& l, Mat4& r) noexcept {
        using std::swap;
        swap(l.data, r.data);
    }

    // Multiplies with Vec3 (divided by the last coordinate)
    auto transform(Vec3<T> const& v) const -> Vec3<T> {
        auto xyz = Vec3<T>(
            data[0] * v.x + data[1] * v.y + data[2] * v.z + data[3],
            data[4] * v.x + data[5] * v.y + data[6] * v.z + data[7],
            data[8] * v.x + data[9] * v.y + data[10] * v.z + data[11]
        );
        auto w = data[12] * v.x + data[13] * v.y + data[14] * v.z + data[15];
        return xyz / w;
    }

    // Constructs a translation matrix
    static auto translate(Vec3<T> const& v) -> Mat4 {
        auto res = Mat4(T(1));
        res.data[3] = v.x;
        res.data[7] = v.y;
        res.data[11] = v.z;
        return res;
    }

    // Constructs a rotation matrix
    static auto rotate(T radians, Vec3<T> const& v) -> Mat4 {
        v.normalize();
        auto s = std::sin(radians), c = std::cos(radians), t = T(1) - c;
        auto res = Mat4();
        res.data[0] = t * v.x * v.x + c;
        res.data[1] = t * v.x * v.y - s * v.z;
        res.data[2] = t * v.x * v.z + s * v.y;
        res.data[4] = t * v.x * v.y + s * v.z;
        res.data[5] = t * v.y * v.y + c;
        res.data[6] = t * v.y * v.z - s * v.x;
        res.data[8] = t * v.x * v.z - s * v.y;
        res.data[9] = t * v.y * v.z + s * v.x;
        res.data[10] = t * v.z * v.z + c;
        res.data[15] = T(1);
        return res;
    }

    // Constructs a perspective projection matrix
    static auto perspective(T fov, T aspect, T near, T far) -> Mat4 {
        auto f = T(1) / std::tan(fov / T(2));
        auto a = near - far;
        auto res = Mat4();
        res.data[0] = f / aspect;
        res.data[5] = f;
        res.data[10] = (far + near) / a;
        res.data[11] = T(2) * far * near / a;
        res.data[14] = T(-1);
        return res;
    }

    // Constructs an orthogonal projection matrix
    static auto ortho(T left, T right, T bottom, T top, T near, T far) -> Mat4 {
        auto a = right - left;
        auto b = top - bottom;
        auto c = far - near;
        auto res = Mat4();
        res.data[0] = T(2) / a;
        res.data[3] = -(right + left) / a;
        res.data[5] = T(2) / b;
        res.data[7] = -(top + bottom) / b;
        res.data[10] = T(-2) / c;
        res.data[11] = -(far + near) / c;
        res.data[15] = T(1);
        return res;
    }
};

export using Mat4f = Mat4<float>;
export using Mat4d = Mat4<double>;

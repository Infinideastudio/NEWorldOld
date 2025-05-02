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
    std::array<T, 16> data = {};

    // Zero matrix constructor
    Mat4() = default;
    // Identity matrix constructor
    Mat4(T x) {
        data[0] = data[5] = data[10] = data[15] = x;
    }

    auto operator[](size_t index) -> T* {
        return data + index * 4;
    }

    auto operator*(const Mat4& rhs) const -> Mat4 {
        Mat4 res;
        res.data[0] = data[0] * rhs.data[0] + data[1] * rhs.data[4] + data[2] * rhs.data[8] + data[3] * rhs.data[12];
        res.data[1] = data[0] * rhs.data[1] + data[1] * rhs.data[5] + data[2] * rhs.data[9] + data[3] * rhs.data[13];
        res.data[2] = data[0] * rhs.data[2] + data[1] * rhs.data[6] + data[2] * rhs.data[10] + data[3] * rhs.data[14];
        res.data[3] = data[0] * rhs.data[3] + data[1] * rhs.data[7] + data[2] * rhs.data[11] + data[3] * rhs.data[15];
        res.data[4] = data[4] * rhs.data[0] + data[5] * rhs.data[4] + data[6] * rhs.data[8] + data[7] * rhs.data[12];
        res.data[5] = data[4] * rhs.data[1] + data[5] * rhs.data[5] + data[6] * rhs.data[9] + data[7] * rhs.data[13];
        res.data[6] = data[4] * rhs.data[2] + data[5] * rhs.data[6] + data[6] * rhs.data[10] + data[7] * rhs.data[14];
        res.data[7] = data[4] * rhs.data[3] + data[5] * rhs.data[7] + data[6] * rhs.data[11] + data[7] * rhs.data[15];
        res.data[8] = data[8] * rhs.data[0] + data[9] * rhs.data[4] + data[10] * rhs.data[8] + data[11] * rhs.data[12];
        res.data[9] = data[8] * rhs.data[1] + data[9] * rhs.data[5] + data[10] * rhs.data[9] + data[11] * rhs.data[13];
        res.data[10] =
            data[8] * rhs.data[2] + data[9] * rhs.data[6] + data[10] * rhs.data[10] + data[11] * rhs.data[14];
        res.data[11] =
            data[8] * rhs.data[3] + data[9] * rhs.data[7] + data[10] * rhs.data[11] + data[11] * rhs.data[15];
        res.data[12] =
            data[12] * rhs.data[0] + data[13] * rhs.data[4] + data[14] * rhs.data[8] + data[15] * rhs.data[12];
        res.data[13] =
            data[12] * rhs.data[1] + data[13] * rhs.data[5] + data[14] * rhs.data[9] + data[15] * rhs.data[13];
        res.data[14] =
            data[12] * rhs.data[2] + data[13] * rhs.data[6] + data[14] * rhs.data[10] + data[15] * rhs.data[14];
        res.data[15] =
            data[12] * rhs.data[3] + data[13] * rhs.data[7] + data[14] * rhs.data[11] + data[15] * rhs.data[15];
        return res;
    }

    auto operator*=(const Mat4& rhs) -> Mat4& {
        *this = *this * rhs;
        return *this;
    }

    // Get transposed matrix
    auto transpose() const -> Mat4 {
        Mat4 res;
        res.data[0] = data[0], res.data[1] = data[4], res.data[2] = data[8], res.data[3] = data[12];
        res.data[4] = data[1], res.data[5] = data[5], res.data[6] = data[9], res.data[7] = data[13];
        res.data[8] = data[2], res.data[9] = data[6], res.data[10] = data[10], res.data[11] = data[14];
        res.data[12] = data[3], res.data[13] = data[7], res.data[14] = data[11], res.data[15] = data[15];
        return res;
    }

    // Swap row r1, row r2
    void swapRows(size_t r1, size_t r2) {
        assert(r1 < 4 && r2 < 4);
        std::swap(data[r1 * 4 + 0], data[r2 * 4 + 0]);
        std::swap(data[r1 * 4 + 1], data[r2 * 4 + 1]);
        std::swap(data[r1 * 4 + 2], data[r2 * 4 + 2]);
        std::swap(data[r1 * 4 + 3], data[r2 * 4 + 3]);
    }

    // Row r *= k
    void multRow(size_t r, T k) {
        assert(r < 4);
        data[r * 4 + 0] *= k;
        data[r * 4 + 1] *= k;
        data[r * 4 + 2] *= k;
        data[r * 4 + 3] *= k;
    }

    // Row dst += row src * k
    void multAndAdd(size_t src, size_t dst, T k) {
        assert(dst < 4 && src < 4);
        data[dst * 4 + 0] += data[src * 4 + 0] * k;
        data[dst * 4 + 1] += data[src * 4 + 1] * k;
        data[dst * 4 + 2] += data[src * 4 + 2] * k;
        data[dst * 4 + 3] += data[src * 4 + 3] * k;
    }

    // Get inverse matrix
    auto inverse() const -> Mat4 {
        Mat4 res = Mat4(T(1));
        for (int i = 0; i < 4; i++) {
            int p = i;
            for (int j = i + 1; j < 4; j++)
                if (abs(data[j * 4 + i]) > abs(data[p * 4 + i]))
                    p = j;
            res.swapRows(i, p);
            swapRows(i, p);
            res.multRow(i, T(1) / data[i * 4 + i]);
            multRow(i, T(1) / data[i * 4 + i]);
            for (int j = i + 1; j < 4; j++) {
                res.multAndAdd(i, j, -data[j * 4 + i]);
                multAndAdd(i, j, -data[j * 4 + i]);
            }
        }
        for (int i = 3; i >= 0; i--) {
            for (int j = 0; j < i; j++) {
                res.multAndAdd(i, j, -data[j * 4 + i]);
                multAndAdd(i, j, -data[j * 4 + i]);
            }
        }
        return res;
    }

    // Construct a translation matrix
    static auto translate(Vec3<T> const& delta) -> Mat4 {
        Mat4 res = Mat4(T(1));
        res.data[3] = delta.x;
        res.data[7] = delta.y;
        res.data[11] = delta.z;
        return res;
    }

    // Construct a rotation matrix
    static auto rotate(T alpha, Vec3<T> const& vec) -> Mat4 {
        Mat4 res;
        vec.normalize();
        T s = std::sin(alpha), c = std::cos(alpha), t = T(1) - c;
        res.data[0] = t * vec.x * vec.x + c;
        res.data[1] = t * vec.x * vec.y - s * vec.z;
        res.data[2] = t * vec.x * vec.z + s * vec.y;
        res.data[4] = t * vec.x * vec.y + s * vec.z;
        res.data[5] = t * vec.y * vec.y + c;
        res.data[6] = t * vec.y * vec.z - s * vec.x;
        res.data[8] = t * vec.x * vec.z - s * vec.y;
        res.data[9] = t * vec.y * vec.z + s * vec.x;
        res.data[10] = t * vec.z * vec.z + c;
        res.data[15] = T(1);
        return res;
    }

    // Construct a perspective projection matrix
    static auto perspective(T fov, T aspect, T near, T far) -> Mat4 {
        Mat4 res;
        T f = T(1) / std::tan(fov / T(2));
        T a = near - far;
        res.data[0] = f / aspect;
        res.data[5] = f;
        res.data[10] = (far + near) / a;
        res.data[11] = T(2) * far * near / a;
        res.data[14] = T(-1);
        return res;
    }

    // Construct an orthogonal projection matrix
    static auto ortho(T left, T right, T bottom, T top, T near, T far) -> Mat4 {
        T a = right - left;
        T b = top - bottom;
        T c = far - near;
        Mat4 res;
        res.data[0] = T(2) / a;
        res.data[3] = -(right + left) / a;
        res.data[5] = T(2) / b;
        res.data[7] = -(top + bottom) / b;
        res.data[10] = T(-2) / c;
        res.data[11] = -(far + near) / c;
        res.data[15] = T(1);
        return res;
    }

    // Multiply with Vec3 (with homogeneous coords divided)
    auto transform(Vec3<T> const& vec) const -> Vec3<T> {
        Vec3<T> res(
            data[0] * vec.x + data[1] * vec.y + data[2] * vec.z + data[3],
            data[4] * vec.x + data[5] * vec.y + data[6] * vec.z + data[7],
            data[8] * vec.x + data[9] * vec.y + data[10] * vec.z + data[11]
        );
        T homoCoord = data[12] * vec.x + data[13] * vec.y + data[14] * vec.z + data[15];
        return res / homoCoord;
    }
};

export using Mat4f = Mat4<float>;
export using Mat4d = Mat4<double>;

export module mat;
import std;
import types;
import vec;

export template <typename T, size_t M, size_t N>
class Matrix {
public:
    std::array<std::array<T, N>, M> elem;

    // Row-by-row constructor.
    template <typename... Ts>
    constexpr Matrix(Ts... xs) requires (sizeof...(Ts) == M)
        :
        elem{xs...} {}

    // Identity matrix constructor.
    constexpr Matrix(T x) requires (M == N)
        :
        Matrix(_map_reduce(std::make_index_sequence<M>{}, _mat_ctor, [&](auto i) {
            return _map_reduce(std::make_index_sequence<N>{}, _row_ctor, [&](auto j) { return i == j ? x : T(0); });
        })) {}

    // Returns reference to the i-th row.
    auto operator[](std::size_t i) const -> std::array<T, N> const& {
        return elem[i];
    }
    auto operator[](std::size_t i) -> std::array<T, N>& {
        return elem[i];
    }

    // Return pointer to the first element.
    auto data() const -> T const* {
        return elem.data()->data();
    }
    auto data() -> T* {
        return elem.data()->data();
    }

    // Lexicographical ordering.
    auto operator<=>(Matrix const&) const = default;

    // Element-wise minus.
    friend auto operator-(Matrix const& m) -> Matrix {
        return _map_reduce(std::make_index_sequence<M>{}, _mat_ctor, [&](auto i) {
            return _map_reduce(std::make_index_sequence<N>{}, _row_ctor, [&](auto j) { return -m.elem[i][j]; });
        });
    }

    // Element-wise addition.
    auto operator+(Matrix const& r) const -> Matrix {
        return _map_reduce(std::make_index_sequence<M>{}, _mat_ctor, [&](auto i) {
            return _map_reduce(std::make_index_sequence<N>{}, _row_ctor, [&](auto j) {
                return elem[i][j] + r.elem[i][j];
            });
        });
    }

    // Element-wise subtraction.
    auto operator-(Matrix const& r) const -> Matrix {
        return _map_reduce(std::make_index_sequence<M>{}, _mat_ctor, [&](auto i) {
            return _map_reduce(std::make_index_sequence<N>{}, _row_ctor, [&](auto j) {
                return elem[i][j] - r.elem[i][j];
            });
        });
    };

    // Matrix multiplication.
    template <size_t O>
    auto operator*(Matrix<T, N, O> const& r) const -> Matrix {
        return _map_reduce(std::make_index_sequence<M>{}, _mat_ctor, [&](auto i) {
            return _map_reduce(std::make_index_sequence<O>{}, _row_ctor, [&](auto j) {
                return _map_reduce(std::make_index_sequence<N>{}, _sum, [&](auto k) {
                    return elem[i][k] * r.elem[k][j];
                });
            });
        });
    }

    // Vector multiplication.
    auto operator*(Vector<T, N> const& v) const -> Vector<T, M> {
        auto _vec_ctor = [](auto... rows) {
            return Vector<T, M>{rows...};
        };
        return _map_reduce(std::make_index_sequence<M>{}, _vec_ctor, [&](auto i) {
            return _map_reduce(std::make_index_sequence<N>{}, _sum, [&](auto j) { return elem[i][j] * v[j]; });
        });
    }

    // Scalar multiplication.
    auto operator*(T value) const -> Matrix {
        return _map_reduce(std::make_index_sequence<M>{}, _mat_ctor, [&](auto i) {
            return _map_reduce(std::make_index_sequence<N>{}, _row_ctor, [&](auto j) { return elem[i][j] * value; });
        });
    }

    // Scalar division.
    auto operator/(T value) const -> Matrix {
        return _map_reduce(std::make_index_sequence<M>{}, _mat_ctor, [&](auto i) {
            return _map_reduce(std::make_index_sequence<N>{}, _row_ctor, [&](auto j) { return elem[i][j] / value; });
        });
    }

    auto operator+=(Matrix const& r) -> Matrix& {
        return *this = *this + r;
    }

    auto operator-=(Matrix const& r) -> Matrix& {
        return *this = *this - r;
    }

    auto operator*=(Matrix const& r) -> Matrix& {
        return *this = *this * r;
    }

    auto operator*=(T value) -> Matrix& {
        return *this = *this * value;
    }

    auto operator/=(T value) -> Matrix& {
        return *this = *this / value;
    }

    // Returns transposed matrix.
    auto transpose() const -> Matrix {
        return _map_reduce(std::make_index_sequence<M>{}, _mat_ctor, [&](auto i) {
            return _map_reduce(std::make_index_sequence<N>{}, _row_ctor, [&](auto j) { return elem[j][i]; });
        });
    }

    // Returns inverse matrix.
    auto inverse() const -> Matrix requires (M == N)
    {
        auto res = Matrix(T(1));
        for (auto i = 0uz; i < M; i++) {
            auto p = i;
            for (auto j = i + 1; j < M; j++)
                if (std::abs(elem[j][i]) > std::abs(elem[p][i]))
                    p = j;
            res._swap_rows(i, p);
            _swap_rows(i, p);
            res._mult_row(i, T(1) / elem[i][i]);
            _mult_row(i, T(1) / elem[i][i]);
            for (auto j = i + 1; j < M; j++) {
                res._mult_and_add(i, j, -elem[j][i]);
                _mult_and_add(i, j, -elem[j][i]);
            }
        }
        for (auto i = M; i-- > 0;) {
            for (auto j = i; j-- > 0;) {
                res._mult_and_add(i, j, -elem[j][i]);
                _mult_and_add(i, j, -elem[j][i]);
            }
        }
        return res;
    }

    // Element-wise function application.
    template <typename U = T, typename F>
    auto map(F f) const -> Matrix<U, M, N> {
        auto _res_mat_ctor = [](auto... rows) {
            return Matrix<U, M, N>{rows...};
        };
        auto _res_row_ctor = [](auto... xs) {
            return std::array<U, N>{xs...};
        };
        return _map_reduce(std::make_index_sequence<M>{}, _res_mat_ctor, [&](auto i) {
            return _map_reduce(std::make_index_sequence<N>{}, _res_row_ctor, [&](auto j) { return f(elem[i][j]); });
        });
    }

    // Swaps two matrices.
    friend void swap(Matrix& l, Matrix& r) noexcept {
        using std::swap;
        swap(l.elem, r.elem);
    }

    // Multiplies with Vec3 (divided by the last coordinate).
    auto transform(Vec3<T> const& v) const -> Vec3<T> requires (M == 4 && N == 4)
    {
        auto res = (*this) * Vec4<T>(v.x(), v.y(), v.z(), T(1));
        return Vec3<T>(res.x(), res.y(), res.z()) / res.w();
    }

    // Constructs a translation matrix.
    static auto translate(Vec3<T> const& v) -> Matrix<T, 4, 4> {
        auto res = Matrix<T, 4, 4>(T(1));
        res.elem[0][3] = v.x();
        res.elem[1][3] = v.y();
        res.elem[2][3] = v.z();
        return res;
    }

    // Constructs a rotation matrix.
    static auto rotate(T radians, Vec3<T> const& v) -> Matrix<T, 4, 4> {
        v.normalize();
        auto s = std::sin(radians), c = std::cos(radians), t = T(1) - c;
        auto res = Matrix<T, 4, 4>(T(1));
        res.elem[0][0] = t * v.x() * v.x() + c;
        res.elem[0][1] = t * v.x() * v.y() - s * v.z();
        res.elem[0][2] = t * v.x() * v.z() + s * v.y();
        res.elem[1][0] = t * v.x() * v.y() + s * v.z();
        res.elem[1][1] = t * v.y() * v.y() + c;
        res.elem[1][2] = t * v.y() * v.z() - s * v.x();
        res.elem[2][0] = t * v.x() * v.z() - s * v.y();
        res.elem[2][1] = t * v.y() * v.z() + s * v.x();
        res.elem[2][2] = t * v.z() * v.z() + c;
        return res;
    }

    // Constructs a perspective projection matrix.
    static auto perspective(T fov, T aspect, T near, T far) -> Matrix<T, 4, 4> {
        auto f = T(1) / std::tan(fov / T(2));
        auto a = near - far;
        auto res = Matrix<T, 4, 4>(T(0));
        res.elem[0][0] = f / aspect;
        res.elem[1][1] = f;
        res.elem[2][2] = (far + near) / a;
        res.elem[2][3] = T(2) * far * near / a;
        res.elem[3][2] = T(-1);
        return res;
    }

    // Constructs an orthogonal projection matrix.
    static auto ortho(T left, T right, T bottom, T top, T near, T far) -> Matrix<T, 4, 4> {
        auto a = right - left;
        auto b = top - bottom;
        auto c = far - near;
        auto res = Matrix<T, 4, 4>(T(0));
        res.elem[0][0] = T(2) / a;
        res.elem[0][3] = -(right + left) / a;
        res.elem[1][1] = T(2) / b;
        res.elem[1][3] = -(top + bottom) / b;
        res.elem[2][2] = T(-2) / c;
        res.elem[2][3] = -(far + near) / c;
        res.elem[3][3] = T(1);
        return res;
    }

private:
    // Uninitialized matrix constructor.
    // constexpr Matrix() = default;

    static constexpr auto _mat_ctor = [](auto... rows) {
        return Matrix{rows...};
    };
    static constexpr auto _row_ctor = [](auto... xs) {
        return std::array<T, N>{xs...};
    };
    static constexpr auto _sum = [](auto... xs) {
        return (xs + ...);
    };

    // Given an index sequence, maps each value using `f` and then applies `g` to all results.
    template <typename G, typename F, size_t... I>
    static constexpr auto _map_reduce(std::index_sequence<I...>, G g, F f) {
        return g(f(I)...);
    }

    void _swap_rows(std::size_t i, std::size_t j) {
        using std::swap;
        swap(elem[i], elem[j]);
    }

    void _mult_row(std::size_t i, T value) {
        _map_reduce(std::make_index_sequence<N>{}, _sum, [&](auto j) { return elem[i][j] *= value; });
    }

    void _mult_and_add(std::size_t j, std::size_t i, T value) {
        _map_reduce(std::make_index_sequence<N>{}, _sum, [&](auto k) { return elem[i][k] += elem[j][k] * value; });
    }
};

export template <typename T>
using Mat2 = Matrix<T, 2, 2>;

export using Mat2f = Mat2<float_t>;
export using Mat2d = Mat2<double_t>;

export template <typename T>
using Mat3 = Matrix<T, 3, 3>;

export using Mat3f = Mat3<float_t>;
export using Mat3d = Mat3<double_t>;

export template <typename T>
using Mat4 = Matrix<T, 4, 4>;

export using Mat4f = Mat4<float_t>;
export using Mat4d = Mat4<double_t>;

export module math:vector;
import std;
import types;

// Small vector template.
export template <typename T, size_t N>
class Vector {
public:
    std::array<T, N> elem = {};

    // Zero vector constructor.
    constexpr Vector() = default;

    // Element-wise constructor.
    template <typename... Ts>
    constexpr Vector(Ts... xs) requires (sizeof...(Ts) == N)
        :
        elem{xs...} {}

    // Same-value constructor.
    constexpr Vector(T x):
        Vector(_map_reduce(std::make_index_sequence<N>{}, _ctor, [&](auto) { return x; })) {}

    // Type conversion constructor.
    template <typename U>
    constexpr explicit Vector(Vector<U, N> r):
        Vector(_map_reduce(std::make_index_sequence<N>{}, _ctor, [&](auto i) { return static_cast<T>(r.elem[i]); })) {}

    // Returns the i-th element.
    auto operator[](size_t i) const -> T {
        return elem[i];
    }
    auto operator[](size_t i) -> T& {
        return elem[i];
    }

    auto x() const -> T requires (N > 0)
    {
        return elem[0];
    }
    auto x() -> T& requires (N > 0)
    {
        return elem[0];
    }
    auto y() const -> T requires (N > 1)
    {
        return elem[1];
    }
    auto y() -> T& requires (N > 1)
    {
        return elem[1];
    }
    auto z() const -> T requires (N > 2)
    {
        return elem[2];
    }
    auto z() -> T& requires (N > 2)
    {
        return elem[2];
    }
    auto w() const -> T requires (N > 3)
    {
        return elem[3];
    }
    auto w() -> T& requires (N > 3)
    {
        return elem[3];
    }

    // Lexicographical ordering.
    auto operator<=>(Vector const&) const = default;

    // Element-wise minus.
    friend auto operator-(Vector v) -> Vector {
        return _map_reduce(std::make_index_sequence<N>{}, _ctor, [&](auto i) { return -v.elem[i]; });
    }

    // Element-wise addition.
    auto operator+(Vector r) const -> Vector {
        return _map_reduce(std::make_index_sequence<N>{}, _ctor, [&](auto i) { return elem[i] + r.elem[i]; });
    }

    // Element-wise subtraction.
    auto operator-(Vector r) const -> Vector {
        return _map_reduce(std::make_index_sequence<N>{}, _ctor, [&](auto i) { return elem[i] - r.elem[i]; });
    };

    // Element-wise multiplication.
    auto operator*(Vector r) const -> Vector {
        return _map_reduce(std::make_index_sequence<N>{}, _ctor, [&](auto i) { return elem[i] * r.elem[i]; });
    };

    // Element-wise division.
    auto operator/(Vector r) const -> Vector {
        return _map_reduce(std::make_index_sequence<N>{}, _ctor, [&](auto i) { return elem[i] / r.elem[i]; });
    };

    // Scalar multiplication.
    auto operator*(T value) const -> Vector {
        return _map_reduce(std::make_index_sequence<N>{}, _ctor, [&](auto i) { return elem[i] * value; });
    }

    // Scalar division.
    auto operator/(T value) const -> Vector {
        return _map_reduce(std::make_index_sequence<N>{}, _ctor, [&](auto i) { return elem[i] / value; });
    }

    // Element-wise addition assignment.
    auto operator+=(Vector r) -> Vector& {
        return *this = *this + r;
    }

    // Element-wise subtraction assignment.
    auto operator-=(Vector r) -> Vector& {
        return *this = *this - r;
    }

    // Element-wise multiplication assignment.
    auto operator*=(Vector r) -> Vector& {
        return *this = *this * r;
    }

    // Element-wise division assignment.
    auto operator/=(Vector r) -> Vector& {
        return *this = *this / r;
    }

    // Scalar multiplication assignment.
    auto operator*=(T value) -> Vector& {
        return *this = *this * value;
    }

    // Scalar division assignment.
    auto operator/=(T value) -> Vector& {
        return *this = *this / value;
    }

    // Dot product.
    auto dot(Vector r) const -> T {
        return _map_reduce(std::make_index_sequence<N>{}, _sum, [&](auto i) { return elem[i] * r.elem[i]; });
    }

    // Element-wise function application.
    template <typename U = T, typename F>
    auto map(F f) const -> Vector<U, N> {
        auto _res_ctor = [](auto... rows) {
            return Vector<U, N>{rows...};
        };
        return _map_reduce(std::make_index_sequence<N>{}, _res_ctor, [&](auto i) { return f(elem[i]); });
    }

    // Swaps two vectors.
    friend void swap(Vector& l, Vector& r) noexcept {
        using std::swap;
        swap(l.elem, r.elem);
    }

    // Returns the square of vector length.
    auto length_sqr() const -> T {
        return _map_reduce(std::make_index_sequence<N>{}, _sum, [&](auto i) { return elem[i] * elem[i]; });
    }

    // Returns vector length.
    template <size_t... I>
    auto length() const -> T {
        return std::sqrt(length_sqr());
    }

    // Returns normalized vector.
    template <size_t... I>
    auto normalize() const -> Vector {
        return *this / length();
    }

    // Returns floored vector.
    template <typename U = T>
    auto floor() const -> Vector<U, N> {
        return map<U>([](auto x) { return static_cast<U>(std::floor(x)); });
    }

    // Returns ceiled vector.
    template <typename U = T>
    auto ceil() const -> Vector<U, N> {
        return map<U>([](auto x) { return static_cast<U>(std::ceil(x)); });
    }

    // Returns rounded vector.
    template <typename U = T>
    auto round() const -> Vector<U, N> {
        return map<U>([](auto x) { return static_cast<U>(std::round(x)); });
    }

private:
    static constexpr auto _ctor = [](auto... xs) {
        return Vector{xs...};
    };
    static constexpr auto _sum = [](auto... xs) {
        return (xs + ...);
    };

    // Given an index sequence, maps each value using `f` and then applies `g` to all results.
    template <typename G, typename F, size_t... I>
    static constexpr auto _map_reduce(std::index_sequence<I...> is, G g, F f) {
        return g(f(I)...);
    }
};

export template <typename T>
using Vec2 = Vector<T, 2>;

export using Vec2i = Vec2<int32_t>;
export using Vec2u = Vec2<uint32_t>;
export using Vec2i8 = Vec2<int8_t>;
export using Vec2u8 = Vec2<uint8_t>;
export using Vec2i16 = Vec2<int16_t>;
export using Vec2u16 = Vec2<uint16_t>;
export using Vec2i32 = Vec2<int32_t>;
export using Vec2u32 = Vec2<uint32_t>;
export using Vec2i64 = Vec2<int64_t>;
export using Vec2u64 = Vec2<uint64_t>;
export using Vec2f = Vec2<float>;
export using Vec2d = Vec2<double>;

export template <typename T>
using Vec3 = Vector<T, 3>;

export using Vec3i = Vec3<int32_t>;
export using Vec3u = Vec3<uint32_t>;
export using Vec3i8 = Vec3<int8_t>;
export using Vec3u8 = Vec3<uint8_t>;
export using Vec3i16 = Vec3<int16_t>;
export using Vec3u16 = Vec3<uint16_t>;
export using Vec3i32 = Vec3<int32_t>;
export using Vec3u32 = Vec3<uint32_t>;
export using Vec3i64 = Vec3<int64_t>;
export using Vec3u64 = Vec3<uint64_t>;
export using Vec3f = Vec3<float>;
export using Vec3d = Vec3<double>;

export template <typename T>
using Vec4 = Vector<T, 4>;

export using Vec4i = Vec4<int32_t>;
export using Vec4u = Vec4<uint32_t>;
export using Vec4i8 = Vec4<int8_t>;
export using Vec4u8 = Vec4<uint8_t>;
export using Vec4i16 = Vec4<int16_t>;
export using Vec4u16 = Vec4<uint16_t>;
export using Vec4i32 = Vec4<int32_t>;
export using Vec4u32 = Vec4<uint32_t>;
export using Vec4i64 = Vec4<int64_t>;
export using Vec4u64 = Vec4<uint64_t>;
export using Vec4f = Vec4<float>;
export using Vec4d = Vec4<double>;

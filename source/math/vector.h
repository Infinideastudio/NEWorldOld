#pragma once

#include <cmath>
#include <utility>
#include <type_traits>

template <typename T>
constexpr T abs(T arg) noexcept
{
    return arg >= 0 ? arg : -arg;
}

template <size_t d, class T> union Vec;

template <class T>
union alignas(4) Vec<2, T> final
{
    T data[2];
    struct { T x, y; };
    // used as color
    struct { T l, a; };
    struct { T r, g; };
    struct { T less, last; };
    Vec() = default;
    constexpr Vec(T _x, T _y) noexcept :x(_x), y(_y) {}
    template <typename U, std::enable_if_t<std::is_convertible<T, U>::value, int> = 0>
    constexpr Vec(const Vec<2, U>& rhs) noexcept : x(T(rhs.x)), y(T(rhs.y)) {}
    constexpr Vec operator + (const Vec& r) const noexcept
    {
        return Vec(x + r.x, y + r.y);
    }
    constexpr Vec operator - (const Vec& r) const noexcept
    {
        return Vec(x - r.x, y - r.y);
    }
    constexpr Vec operator - () const noexcept
    {
        return Vec(-x, -y);
    }
    template <class T2>
    constexpr Vec operator * (T2&& r) const noexcept
    {
        return Vec(x * std::forward<T2>(r), y  * std::forward<T2>(r));
    }
    template <class T2>
    constexpr Vec operator / (T2&& r) const noexcept
    {
        return Vec(x / std::forward<T2>(r), y / std::forward<T2>(r));
    }
    Vec& operator += (const Vec& r) noexcept
    {
        x += r.x; y += r.y; return *this;
    }
    Vec& operator -= (const Vec& r) noexcept
    {
        x -= r.x; y -= r.y; return *this;
    }
    template <class T2>
    Vec& operator *= (T2&& r) noexcept
    {
        x *= std::forward<T2>(r); y *= std::forward<T2>(r); return *this;
    }
    template <class T2>
    Vec& operator /= (T2&& r) noexcept
    {
        x /= std::forward<T2>(r); y /= std::forward<T2>(r); return *this;
    }
    constexpr T lengthSqr() const noexcept
    {
        return x * x + y * y;
    }
    constexpr bool operator == (const Vec& r) const noexcept
    {
        return (x == r.x) && (y == r.y);
    }
    constexpr bool operator < (const Vec& r) const noexcept
    {
        return lengthSqr() < r.lengthSqr();
    }
    constexpr bool operator > (const Vec& r) const noexcept
    {
        return lengthSqr() > r.lengthSqr();
    }
    constexpr bool operator <= (const Vec& r) const noexcept
    {
        return lengthSqr() <= r.lengthSqr();
    }
    constexpr bool operator >= (const Vec& r) const noexcept
    {
        return lengthSqr() >= r.lengthSqr();
    }
    constexpr T dot(const Vec& r) const noexcept
    {
        return x * r.x + y * r.y;
    }
    void normalize() noexcept
    {
        (*this) /= length();
    }
    T length() noexcept
    {
        return sqrt(lengthSqr());
    }
    double euclideanDistance(const Vec& rhs) const noexcept
    {
        return (*this - rhs).length();
    }
    constexpr T chebyshevDistance(const Vec& rhs) const noexcept
    {
        return std::max(abs(x - rhs.x), abs(y - rhs.y));
    }
    constexpr T manhattanDistance(const Vec& rhs) const noexcept
    {
        return abs(x - rhs.x) + abs(y - rhs.y);
    }
};

template <class T>
union alignas(4) Vec<3, T> final
{
    T data[3];
    struct { T x, y, z; };
    // used as color
    struct { T r, g, b; };
    struct { Vec<2, T> less; T last; };
    Vec<2, T> xy;
    Vec<2, T> rg;
    Vec() = default;
    constexpr Vec(T _x, T _y, T _z) noexcept : x(_x), y(_y), z(_z) {}
    constexpr Vec(const Vec<2, T>& ls, T arg) noexcept : less(ls), last(arg) {}
    template <typename U, std::enable_if_t<std::is_convertible<T, U>::value, int> = 0>
    constexpr Vec(const Vec<3, U>& rhs) noexcept : x(T(rhs.x)), y(T(rhs.y)), z(T(rhs.z))
    {
    }
    constexpr Vec operator + (const Vec& r) const noexcept
    {
        return Vec(x + r.x, y + r.y, z + r.z);
    }
    constexpr Vec operator - (const Vec& r) const noexcept
    {
        return Vec(x - r.x, y - r.y, z - r.z);
    }
    constexpr Vec operator - () const noexcept
    {
        return Vec(-x, -y, -z);
    }
    template <class T2>
    constexpr Vec operator * (T2&& r) const noexcept
    {
        return Vec(x * std::forward<T2>(r), y * std::forward<T2>(r), z * std::forward<T2>(r));
    }
    //cross product
    constexpr Vec operator * (const Vec& r) const noexcept
    {
        return Vec(y * r.z - z * r.y, z * r.x - x * r.z, x * r.y - y * r.x);
    }
    template <class T2>
    constexpr Vec operator / (T2&& r) const noexcept
    {
        return Vec(x / std::forward<T2>(r), y / std::forward<T2>(r), z / std::forward<T2>(r));
    }
    Vec& operator += (const Vec& r) noexcept
    {
        x += r.x; y += r.y; z += r.z; return *this;
    }
    Vec& operator -= (const Vec& r) noexcept
    {
        x -= r.x; y -= r.y; z -= r.z; return *this;
    }
    template <class T2>
    Vec& operator *= (T2&& r) noexcept
    {
        x *= std::forward<T2>(r); y *= std::forward<T2>(r); z *= std::forward<T2>(r); return *this;
    }
    //cross product
    Vec& operator *= (const Vec& r) const noexcept
    {
        *this = Vec(y * r.z - z * r.y, z * r.x - x * r.z, x * r.y - y * r.x); return *this;
    }
    template <class T2>
    Vec& operator /= (T2&& r) noexcept
    {
        x /= std::forward<T2>(r); y /= std::forward<T2>(r); z /= std::forward<T2>(r); return *this;
    }
    constexpr T lengthSqr() const noexcept
    {
        return x * x + y * y + z * z;
    }
    constexpr bool operator == (const Vec& r) const noexcept
    {
        return (x == r.x) && (y == r.y) && (z == r.z);
    }
    constexpr bool operator < (const Vec& r) const noexcept
    {
        return lengthSqr() < r.lengthSqr();
    }
    constexpr bool operator > (const Vec& r) const noexcept
    {
        return lengthSqr() > r.lengthSqr();
    }
    constexpr bool operator <= (const Vec& r) const noexcept
    {
        return lengthSqr() <= r.lengthSqr();
    }
    constexpr bool operator >= (const Vec& r) const noexcept
    {
        return lengthSqr() >= r.lengthSqr();
    }
    constexpr T dot(const Vec& r) const noexcept
    {
        return x * r.x + y * r.y + z * r.z;
    }
    T length() noexcept
    {
        return sqrt(lengthSqr());
    }
    void normalize() noexcept
    {
        (*this) /= length();
    }
    template<class Vec3Type>
    Vec3Type conv() const noexcept
    {
        return Vec3Type(x, y, z);
    }
    double euclideanDistance(const Vec& rhs) const noexcept
    {
        return (*this - rhs).length();
    }
    constexpr T chebyshevDistance(const Vec& rhs) const noexcept
    {
        return std::max(std::max(abs(x - rhs.x), abs(y - rhs.y)), abs(z - rhs.z));
    }
    constexpr T manhattanDistance(const Vec& rhs) const noexcept
    {
        return abs(x - rhs.x) + abs(y - rhs.y) + abs(z - rhs.z);
    }
};

template <class T>
union alignas(4) Vec<4, T> final
{
    T data[4];
    struct { T x, y, z, t; };
    // used as color
    struct { T r, g, b, a; };
    struct { T C, M, Y, K; };
    struct { Vec<3, T> less; T last; };
    Vec<2, T> xy;
    Vec<2, T> rg;
    Vec<3, T> xyz;
    Vec<3, T> rgb;
    Vec() = default;
    constexpr Vec(T _x, T _y, T _z, T _t) noexcept : x(_x), y(_y), z(_z), t(_t) {}
    template <typename U, std::enable_if_t<std::is_convertible<T, U>::value, int> = 0>
    constexpr Vec(const Vec<3, U>& rhs) noexcept : x(T(rhs.x)), y(T(rhs.y)), z(T(rhs.z)), t(T(rhs.t))
    {
    }
    constexpr Vec(const Vec<3, T>& ls, T arg) noexcept : less(ls), last(arg) {}
    constexpr Vec(const Vec<2, T>& ls, T arg, T arg0) noexcept : less(ls, arg), last(arg0) {}
    constexpr Vec operator + (const Vec& r) const noexcept
    {
        return Vec(x + r.x, y + r.y, z + r.z, t + r.t);
    }
    constexpr Vec operator - (const Vec& r) const noexcept
    {
        return Vec(x - r.x, y - r.y, z - r.z, t - r.t);
    }
    constexpr Vec operator - () const noexcept
    {
        return Vec(-x, -y, -z, t);
    }
    template <class T2>
    constexpr Vec operator * (T2&& r) const noexcept
    {
        return Vec(x * std::forward<T2>(r), y * std::forward<T2>(r), z * std::forward<T2>(r), t * std::forward<T2>(r));
    }
    template <class T2>
    constexpr Vec operator / (T2&& r) const noexcept
    {
        return Vec(x / std::forward<T2>(r), y / std::forward<T2>(r), z / std::forward<T2>(r), t / std::forward<T2>(r));
    }
    Vec& operator += (const Vec& r) noexcept
    {
        x += r.x; y += r.y; z += r.z; t += r.t; return *this;
    }
    Vec& operator -= (const Vec& r) noexcept
    {
        x -= r.x; y -= r.y; z -= r.z; t -= r.t; return *this;
    }
    template <class T2>
    Vec& operator *= (T2&& r) noexcept
    {
        x *= std::forward<T2>(r); y *= std::forward<T2>(r); z *= std::forward<T2>(r); t *= std::forward<T2>(r); return *this;
    }
    template <class T2>
    Vec& operator /= (T2&& r) noexcept
    {
        x /= std::forward<T2>(r); y /= std::forward<T2>(r); z /= std::forward<T2>(r); t *= std::forward<T2>(r); return *this;
    }
    constexpr T lengthSqr() const noexcept
    {
        return x * x + y * y + z * z + t * t;
    }
    constexpr bool operator == (const Vec& r) const noexcept
    {
        return (x == r.x) && (y == r.y) && (z == r.z) && (t == r.t);
    }
    constexpr bool operator < (const Vec& r) const noexcept
    {
        return lengthSqr() < r.lengthSqr();
    }
    constexpr bool operator > (const Vec& r) const noexcept
    {
        return lengthSqr() > r.lengthSqr();
    }
    constexpr bool operator <= (const Vec& r) const noexcept
    {
        return lengthSqr() <= r.lengthSqr();
    }
    constexpr bool operator >= (const Vec& r) const noexcept
    {
        return lengthSqr() >= r.lengthSqr();
    }
    constexpr T dot(const Vec& r) const noexcept
    {
        return x * r.x + y * r.y + z * r.z + t * r.t;
    }
    void normalize() noexcept
    {
        (*this) /= length();
    }
    T length() noexcept
    {
        return sqrt(lengthSqr());
    }
    double euclideanDistance(const Vec& rhs) const noexcept
    {
        return (*this - rhs).length();
    }
    constexpr T chebyshevDistance(const Vec& rhs) const noexcept
    {
        return std::max(std::max(std::max(abs(x - rhs.x), abs(y - rhs.y)), abs(z - rhs.z)), abs(t - rhs.t));
    }
    constexpr T manhattanDistance(const Vec& rhs) const noexcept
    {
        return abs(x - rhs.x) + abs(y - rhs.y) + abs(z - rhs.z) + abs(t - rhs.t);
    }
};
using Vec2i = Vec<2, int>;
using Vec2f = Vec<2, float>;
using Vec2d = Vec<2, double>;
template <class T>
using Vec2 = Vec<2, T>;
using Vec3i = Vec<3, int>;
using Vec3f = Vec<3, float>;
using Vec3d = Vec<3, double>;
template <class T>
using Vec3 = Vec<3, T>;
using Vec4i = Vec<4, int>;
using Vec4f = Vec<4, float>;
using Vec4d = Vec<4, double>;
template <class T>
using Vec4 = Vec<4, T>;

template <size_t d, class T>
union Vec final
{
    T data[d];
    struct
    {
        Vec<d - 1, T> less;
        T last;
    };
    Vec() = default;
    template <class ...T2>
    constexpr Vec(const T& a1, T2&&... args) noexcept :
    data{ a1, std::forward<T2>(args)... } {}
    constexpr Vec(const Vec<d - 1, T>& _less, const T& _last) noexcept :
        less(_less), last(_last) {}
    template <typename U, std::enable_if_t<std::is_convertible<T, U>::value, int> = 0>
    constexpr Vec(const Vec<d, U>& rhs) noexcept : less(Vec(rhs.less)), last(T(rhs.last))
    {
    }
    template <size_t d2, class ...T2>
    constexpr Vec(const Vec<d2, T>& ptr, const T& arg, const T& arg2, T2&&...args) noexcept :
        Vec(Vec<d2 + 1, T>(ptr, arg), arg2, std::forward<T2>(args)...) {}
    constexpr Vec operator + (const Vec& r) const noexcept
    {
        return Vec(less + r.less, last + r.last);
    }
    constexpr Vec operator - (const Vec& r) const noexcept
    {
        return Vec(less - r.less, last - r.last);
    }
    constexpr Vec operator - () const noexcept
    {
        return Vec(-less, -last);
    }
    template <class T2>
    constexpr Vec operator * (T2&& r) const noexcept
    {
        return Vec(less * std::forward<T2>(r), last * std::forward<T2>(r));
    }
    template <class T2>
    constexpr Vec operator / (T2&& r) const noexcept
    {
        return Vec(less / std::forward<T2>(r), last / std::forward<T2>(r));
    }
    Vec& operator += (const Vec& r) noexcept
    {
        less += r.less; last += r.last; return *this;
    }
    Vec& operator -= (const Vec& r) noexcept
    {
        less -= r.less; last -= r.last; return *this;
    }
    template <class T2>
    Vec& operator *= (T2&& r) noexcept
    {
        less *= std::forward<T2>(r); last *= std::forward<T2>(r); return *this;
    }
    template <class T2>
    Vec& operator /= (T2&& r) noexcept
    {
        less /= std::forward<T2>(r); last /= std::forward<T2>(r); return *this;
    }
    constexpr T lengthSqr() const noexcept
    {
        return less.lengthSqr() + last * last;
    }
    constexpr bool operator == (const Vec& r) const noexcept
    {
        return (less == r.less) && (last == r.last);
    }
    constexpr bool operator < (const Vec& r) const noexcept
    {
        return lengthSqr() < r.lengthSqr();
    }
    constexpr bool operator > (const Vec& r) const noexcept
    {
        return lengthSqr() > r.lengthSqr();
    }
    constexpr bool operator <= (const Vec& r) const noexcept
    {
        return lengthSqr() <= r.lengthSqr();
    }
    constexpr bool operator >= (const Vec& r) const noexcept
    {
        return lengthSqr() >= r.lengthSqr();
    }
    constexpr T dot(const Vec& r) const noexcept
    {
        return less.dot(r.less) + last * r.last;
    }
    void normalize() noexcept
    {
        (*this) /= length();
    }
    T length()
    {
        return sqrt(lengthSqr());
    }
    double euclideanDistance(const Vec& rhs) const noexcept
    {
        return (*this - rhs).length();
    }
    constexpr T chebyshevDistance(const Vec& rhs) const noexcept
    {
        return std::max(less.chebyshevDistance(rhs.less), abs(last - rhs.last));
    }
    constexpr T manhattanDistance(const Vec& rhs) const noexcept
    {
        return less.manhattanDistance(rhs.less) + abs(last - rhs.last);
    }
};

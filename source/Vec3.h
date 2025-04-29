/*
* NEWorld: A free game with similar rules to Minecraft.
* Copyright (C) 2016 NEWorld Team
*
* This file is part of NEWorld.
* NEWorld is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* NEWorld is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with NEWorld.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef VEC3_H_
#define VEC3_H_

#include <cmath>
#include <type_traits>
#include <utility>
#include <numbers>

template <typename T>
struct Vec3 {
    T x, y, z;

    // Get the square of vector length
    T lengthSqr() const {
        return x * x + y * y + z * z;
    }

    // Get vector length
    double length() const {
        return std::sqrt(double(lengthSqr()));
    }

    // Get the Euclidean Distance between vectors
    double euclideanDistance(const Vec3& rhs) const {
        return (*this - rhs).length();
    }

    // Get the Chebyshev Distance between vectors
    T chebyshevDistance(const Vec3& rhs) const {
        return std::max(std::max(std::abs(x - rhs.x), std::abs(y - rhs.y)), std::abs(z - rhs.z));
    }

    // Get the Manhattan Distance between vectors
    T manhattanDistance(const Vec3& rhs) const {
        return std::abs(x - rhs.x) + std::abs(y - rhs.y) + std::abs(z - rhs.z);
    }

    // Normalize vector
    void normalize() {
        double l = length();
        x = static_cast<T>(x / l);
        y = static_cast<T>(y / l);
        z = static_cast<T>(z / l);
    }

    bool operator<(const Vec3& rhs) const {
        if (x != rhs.x)
            return x < rhs.x;
        if (y != rhs.y)
            return y < rhs.y;
        if (z != rhs.z)
            return z < rhs.z;
        return false;
    }

    bool operator==(const Vec3& rhs) const {
        return x == rhs.x && y == rhs.y && z == rhs.z;
    }

    Vec3& operator+=(const Vec3& rhs) {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    Vec3& operator-=(const Vec3& rhs) {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    Vec3<T>& operator*=(T value) {
        x *= value;
        y *= value;
        z *= value;
        return *this;
    }

    Vec3<T>& operator/=(T value) {
        x /= value;
        y /= value;
        z /= value;
        return *this;
    }

    Vec3<T> operator*(T value) const {
        return Vec3<T>(x * value, y * value, z * value);
    }

    Vec3<T> operator/(T value) const {
        return Vec3<T>(x / value, y / value, z / value);
    }

    bool operator!=(const Vec3& rhs) const {
        return !(rhs == *this);
    }

    friend Vec3<T> operator-(const Vec3<T>& vec) {
      return Vec3<T>(-vec.x, -vec.y, -vec.z);
    }

    const Vec3<T> operator+(const Vec3<T>& rhs) const {
        Vec3<T> tmp(*this);
        tmp += rhs;
        return tmp;
    };

    const Vec3<T> operator-(const Vec3<T>& rhs) const {
        Vec3<T> tmp(*this);
        tmp -= rhs;
        return tmp;
    };

    const Vec3<T> operator*(const Vec3<T>& rhs) const {
        Vec3<T> tmp(*this);
        tmp *= rhs;
        return tmp;
    };

    const Vec3<T> operator/(const Vec3<T>& rhs) const {
        Vec3<T> tmp(*this);
        tmp /= rhs;
        return tmp;
    };

    friend void swap(Vec3& lhs, Vec3& rhs) {
        swap(lhs.x, rhs.x);
        swap(lhs.y, rhs.y);
        swap(lhs.z, rhs.z);
    }

    template <typename... ArgType, typename Func>
    void for_each(Func func, ArgType&&... args) const {
        func(x, std::forward<ArgType>(args)...);
        func(y, std::forward<ArgType>(args)...);
        func(z, std::forward<ArgType>(args)...);
    }

    template <typename... ArgType, typename Func>
    void for_each(Func func, ArgType&&... args) {
        func(x, std::forward<ArgType>(args)...);
        func(y, std::forward<ArgType>(args)...);
        func(z, std::forward<ArgType>(args)...);
    }

    template <typename Func>
    Vec3<T> transform(Func func) const {
        return Vec3<T>(func(x), func(y), func(z));
    }

    template <typename Func>
    Vec3<T> transform(Func func) {
        return Vec3<T>(func(x), func(y), func(z));
    }

    template <typename Func>
    static void for_range(T begin, T end, Func func) {
        Vec3<T> tmp;
        for (tmp.x = begin; tmp.x < end; ++tmp.x)
            for (tmp.y = begin; tmp.y < end; ++tmp.y)
                for (tmp.z = begin; tmp.z < end; ++tmp.z)
                    func(tmp);
    }

    template <typename Func>
    static void for_range(const Vec3<T>& begin, const Vec3<T>& end, Func func) {
        Vec3<T> tmp;
        for (tmp.x = begin.x; tmp.x < end.x; ++tmp.x)
            for (tmp.y = begin.y; tmp.y < end.y; ++tmp.y)
                for (tmp.z = begin.z; tmp.z < end.z; ++tmp.z)
                    func(tmp);
    }

    template <typename U, U base>
    U encode() {
        return (x * base + y) * base + z;
    }

    template <typename U, U base>
    static Vec3<U> decode(T arg) {
        U z = arg % base;
        arg /= base;
        return Vec3<U>(arg / base, arg % base, z);
    }
};

using Vec3i = Vec3<int>;
using Vec3f = Vec3<float>;
using Vec3d = Vec3<double>;

#endif // !VEC3_H_

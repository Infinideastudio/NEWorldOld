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
#include <numbers>
#include <type_traits>
#include <utility>

template <typename T>
class Vec3 {
public:
    T x, y, z;

    Vec3(T x = T(0), T y = T(0), T z = T(0)):
        x(x),
        y(y),
        z(z) {}

    // Returns the square of vector length
    T lengthSqr() const {
        return x * x + y * y + z * z;
    }

    // Returns vector length
    T length() const {
        return std::sqrt(lengthSqr());
    }

    // Normalize vector
    Vec3 normalize() const {
        return *this / length();
    }

    bool operator==(Vec3 const& rhs) const {
        return x == rhs.x && y == rhs.y && z == rhs.z;
    }

    Vec3& operator+=(Vec3 const& rhs) {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    Vec3& operator-=(Vec3 const& rhs) {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    Vec3& operator*=(T value) {
        x *= value;
        y *= value;
        z *= value;
        return *this;
    }

    Vec3& operator/=(T value) {
        x /= value;
        y /= value;
        z /= value;
        return *this;
    }

    Vec3 operator*(T value) const {
        return Vec3(x * value, y * value, z * value);
    }

    Vec3 operator/(T value) const {
        return Vec3(x / value, y / value, z / value);
    }

    bool operator!=(Vec3 const& rhs) const {
        return !(rhs == *this);
    }

    friend Vec3 operator-(Vec3 const& vec) {
        return Vec3(-vec.x, -vec.y, -vec.z);
    }

    Vec3 operator+(Vec3 const& rhs) const {
        Vec3 res(*this);
        res += rhs;
        return res;
    };

    Vec3 operator-(Vec3 const& rhs) const {
        Vec3 res(*this);
        res -= rhs;
        return res;
    };

    Vec3 operator*(Vec3 const& rhs) const {
        Vec3 res(*this);
        res *= rhs;
        return res;
    };

    Vec3 operator/(Vec3 const& rhs) const {
        Vec3 res(*this);
        res /= rhs;
        return res;
    };

    template <typename Func>
    Vec3 map(Func func) const {
        return Vec3(func(x), func(y), func(z));
    }

    friend void swap(Vec3& lhs, Vec3& rhs) {
        using std::swap;
        swap(lhs.x, rhs.x);
        swap(lhs.y, rhs.y);
        swap(lhs.z, rhs.z);
    }
};

using Vec3i = Vec3<int>;
using Vec3f = Vec3<float>;
using Vec3d = Vec3<double>;

#endif // !VEC3_H_

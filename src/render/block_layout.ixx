module;

#include <glad/gl.h>

export module render:block_layout;
import std;
import types;
import debug;
import math;

namespace render {

// Provides information about using `T` as a block interface field.
// This gets a snake-case name since it is more like a *type-level function* than an actual class.
export template <typename T>
requires std::is_standard_layout_v<T> && std::is_default_constructible_v<T> && std::is_trivially_copyable_v<T>
struct block_interface_type_info {};

// Tests if `block_interface_type_info` has the necessary specialization for `T`.
// Specializations should guarantee that the defined GL type layout matches the GLSL `std140` requirements.
export template <typename T>
concept vertex_attrib_type = requires (T t) {
    // The size of the type in bytes.
    { block_interface_type_info<T>::size } -> std::convertible_to<size_t>;

    // The alignment of the type in bytes.
    { block_interface_type_info<T>::alignment } -> std::convertible_to<size_t>;
};

// Fundamental types.
template <>
struct block_interface_type_info<bool> {
    static constexpr auto size = sizeof(bool);
    static constexpr auto alignment = size;
};

template <>
struct block_interface_type_info<int8_t> {
    static constexpr auto size = sizeof(int8_t);
    static constexpr auto alignment = size;
};

template <>
struct block_interface_type_info<uint8_t> {
    static constexpr auto size = sizeof(uint8_t);
    static constexpr auto alignment = size;
};

template <>
struct block_interface_type_info<int16_t> {
    static constexpr auto size = sizeof(int16_t);
    static constexpr auto alignment = size;
};

template <>
struct block_interface_type_info<uint16_t> {
    static constexpr auto size = sizeof(uint16_t);
    static constexpr auto alignment = size;
};

template <>
struct block_interface_type_info<int32_t> {
    static constexpr auto size = sizeof(int32_t);
    static constexpr auto alignment = size;
};

template <>
struct block_interface_type_info<uint32_t> {
    static constexpr auto size = sizeof(uint32_t);
    static constexpr auto alignment = size;
};

template <>
struct block_interface_type_info<float> {
    static constexpr auto size = sizeof(float);
    static constexpr auto alignment = size;
};

template <>
struct block_interface_type_info<double> {
    static constexpr auto size = sizeof(double);
    static constexpr auto alignment = size;
};

// Vector types.
template <typename T>
requires vertex_attrib_type<T>
struct block_interface_type_info<Vector<T, 1>> {
    static constexpr auto size = sizeof(T) * 1;
    static constexpr auto alignment = size;
};

template <typename T>
requires vertex_attrib_type<T>
struct block_interface_type_info<Vector<T, 2>> {
    static constexpr auto size = sizeof(T) * 2;
    static constexpr auto alignment = size;
};

template <typename T>
requires vertex_attrib_type<T>
struct block_interface_type_info<Vector<T, 3>> {
    static constexpr auto size = sizeof(T) * 4; // Required by std140.
    static constexpr auto alignment = size;
};

template <typename T>
requires vertex_attrib_type<T>
struct block_interface_type_info<Vector<T, 4>> {
    static constexpr auto size = sizeof(T) * 4;
    static constexpr auto alignment = size;
};

// Array types.
constexpr auto _std140_padded_size(size_t size) -> size_t {
    constexpr auto SIZE_VEC4F = block_interface_type_info<Vec4f>::size;
    auto remainder = size % SIZE_VEC4F;
    if (remainder == 0) {
        return size;
    }
    return size + SIZE_VEC4F - remainder; // Required by std140.
}

template <typename T, size_t N>
requires vertex_attrib_type<T>
struct block_interface_type_info<std::array<T, N>> {
    static constexpr auto alignment = _std140_padded_size(block_interface_type_info<T>::size);
    static constexpr auto size = alignment * N;
};

// Matrix types.
template <typename T, size_t M, size_t N>
requires vertex_attrib_type<T>
struct block_interface_type_info<Matrix<T, M, N>>: block_interface_type_info<std::array<Vector<T, N>, M>> {};

}

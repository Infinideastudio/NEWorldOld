module;

#include <glad/gl.h>

export module render:block_layout;
import std;
import types;
import debug;
import math;

// The goal of this submodule is to take a `constexpr` description of an GLSL block interface
// (presumably an `std::array` containing pairs of names and types) and generate at compile time
// a type-safe API for data access.
//
// To achieve this, the generator should emit an array consisting of entries each containing the
// following information:
//
// * The name of the field.
// * The type of the field.
// * The offset of the field in the block interface.
// * The size of the field in bytes.
namespace render {

// Provides information about using `T` as a block interface field.
// This gets a snake-case name since it is more like a *type-level function* than an actual class.
template <typename T>
requires std::is_standard_layout_v<T> && std::is_default_constructible_v<T> && std::is_trivially_copyable_v<T>
struct block_interface_type_info {};

// Tests if `block_interface_type_info` has the necessary specialization for `T`.
// Specializations should guarantee that the defined GL type layout matches the GLSL `std140` requirements.
template <typename T>
concept block_interface_type = requires (T t) {
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
constexpr auto _std140_padded_count(size_t count) -> size_t {
    return count == 3 ? 4 : count;
}

template <size_t N>
requires (2 <= N && N <= 4)
struct block_interface_type_info<Vector<bool, N>> {
    static constexpr auto size = sizeof(bool) * _std140_padded_count(N);
    static constexpr auto alignment = size;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct block_interface_type_info<Vector<int32_t, N>> {
    static constexpr auto size = sizeof(int32_t) * _std140_padded_count(N);
    static constexpr auto alignment = size;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct block_interface_type_info<Vector<uint32_t, N>> {
    static constexpr auto size = sizeof(uint32_t) * _std140_padded_count(N);
    static constexpr auto alignment = size;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct block_interface_type_info<Vector<float, N>> {
    static constexpr auto size = sizeof(float) * _std140_padded_count(N);
    static constexpr auto alignment = size;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct block_interface_type_info<Vector<double, N>> {
    static constexpr auto size = sizeof(double) * _std140_padded_count(N);
    static constexpr auto alignment = size;
};

// Matrix types.
constexpr auto _std140_padded_size(size_t size) -> size_t {
    constexpr auto SIZE_VEC4F = block_interface_type_info<Vec4f>::size;
    auto remainder = size % SIZE_VEC4F;
    if (remainder == 0) {
        return size;
    }
    return size + SIZE_VEC4F - remainder;
}

template <typename T, size_t M, size_t N>
requires block_interface_type<Vector<T, N>> && (2 <= M && M <= 4)
struct block_interface_type_info<Matrix<T, M, N>> {
    static constexpr auto alignment = _std140_padded_size(block_interface_type_info<Vector<T, N>>::size);
    static constexpr auto size = alignment * M;
};

// Array types.
template <typename T, size_t N>
requires block_interface_type<T>
struct block_interface_type_info<std::array<T, N>> {
    static constexpr auto alignment = _std140_padded_size(block_interface_type_info<T>::size);
    static constexpr auto size = alignment * N;
};

// Aggregate types.
template <typename... T>
requires (block_interface_type<T> && ...)
struct block_interface_type_info<std::tuple<T...>> {
    static constexpr auto size = (block_interface_type_info<T>::size + ...);
    static constexpr auto alignment = (block_interface_type_info<T>::alignment + ...);
};

/*
enum class BlockInterfaceBaseType : GLenum {
    BOOL = GL_BOOL,
    INT = GL_INT,
    UINT = GL_UNSIGNED_INT,
    FLOAT = GL_FLOAT,
    DOUBLE = GL_DOUBLE,
    BVEC2 = GL_BOOL_VEC2,
    BVEC3 = GL_BOOL_VEC3,
    BVEC4 = GL_BOOL_VEC4,
    IVEC2 = GL_INT_VEC2,
    IVEC3 = GL_INT_VEC3,
    IVEC4 = GL_INT_VEC4,
    UVEC2 = GL_UNSIGNED_INT_VEC2,
    UVEC3 = GL_UNSIGNED_INT_VEC3,
    UVEC4 = GL_UNSIGNED_INT_VEC4,
    VEC2 = GL_FLOAT_VEC2,
    VEC3 = GL_FLOAT_VEC3,
    VEC4 = GL_FLOAT_VEC4,
    DVEC2 = GL_DOUBLE_VEC2,
    DVEC3 = GL_DOUBLE_VEC3,
    DVEC4 = GL_DOUBLE_VEC4,
    MAT2 = GL_FLOAT_MAT2,
    MAT2X3 = GL_FLOAT_MAT2x3,
    MAT2X4 = GL_FLOAT_MAT2x4,
    MAT3X2 = GL_FLOAT_MAT3x2,
    MAT3 = GL_FLOAT_MAT3,
    MAT3X4 = GL_FLOAT_MAT3x4,
    MAT4X2 = GL_FLOAT_MAT4x2,
    MAT4X3 = GL_FLOAT_MAT4x3,
    MAT4 = GL_FLOAT_MAT4,
};
*/

}

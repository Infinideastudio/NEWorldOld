module;

#include <glad/gl.h>

export module render:attrib_layout;
import std;
import types;
import debug;
import math;
import :types;

// # Attribute layouts
//
// The goal of this submodule is to take a `constexpr` description of a (vertex or instance)
// attribute array and return, for each query, the offset and size of the
// field with the requested name in the interface block.
//
// ## Example usage
//
// ```cpp
// using Coord = spec::Coord<spec::Vec3u8>;
// using TexCoord = spec::TexCoord<spec::Vec3f>;
// using Color = spec::Color<spec::Vec3f>;
// using Material = spec::Material<spec::UInt>;
//
// constexpr auto result = interleave_v<Coord, TexCoord, Color, Material>;
// static_assert(result.offsets[2] == 15);
// static_assert(result.sizes[2] == 12);
// static_assert(std::is_same_v<std::tuple_element_t<2, typename decltype(result)::cpp_types>, Vec3f>);
// ```
namespace render::attrib_layout {

// Tags for attribute scalar types.
// See: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glVertexAttribPointer.xhtml
enum class AttribScalarType : GLenum {
    BYTE = GL_BYTE,
    UNSIGNED_BYTE = GL_UNSIGNED_BYTE,
    SHORT = GL_SHORT,
    UNSIGNED_SHORT = GL_UNSIGNED_SHORT,
    INT = GL_INT,
    UNSIGNED_INT = GL_UNSIGNED_INT,
    HALF_FLOAT = GL_HALF_FLOAT,
    FLOAT = GL_FLOAT,
    DOUBLE = GL_DOUBLE,
    FIXED = GL_FIXED,
    INT_2_10_10_10_REV = GL_INT_2_10_10_10_REV,
    UNSIGNED_INT_2_10_10_10_REV = GL_UNSIGNED_INT_2_10_10_10_REV,
    UNSIGNED_INT_10F_11F_11F_REV = GL_UNSIGNED_INT_10F_11F_11F_REV,
};

// Possible type conversion modes for attributes.
// See: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glVertexAttribPointer.xhtml
enum class Conversion { INT, FLOAT, FLOAT_NORMALIZE, DOUBLE };

// Concept for attribute layout specifiers.
template <typename T>
concept Layout = requires {
    { T::base_type } -> std::convertible_to<GLenum>;
    { T::elem_count } -> std::convertible_to<GLuint>;
    { T::mode } -> std::convertible_to<Conversion>;
    { T::size } -> std::convertible_to<size_t>;
};

// Aattribute layout specifiers.
namespace spec {

    // Returns the base type of a scalar type.
    consteval auto _scalar_base_type(AttribScalarType tag) -> GLenum {
        return static_cast<GLenum>(tag);
    }

    // Returns the packed component count of a scalar type.
    consteval auto _scalar_packed_count(AttribScalarType tag) -> GLuint {
        switch (tag) {
            case AttribScalarType::INT_2_10_10_10_REV:
            case AttribScalarType::UNSIGNED_INT_2_10_10_10_REV:
                return 4;
            case AttribScalarType::UNSIGNED_INT_10F_11F_11F_REV:
                return 3;
            default:
                return 1;
        }
    }

    // Returns the conversion mode of a scalar type.
    consteval auto _scalar_mode(AttribScalarType tag) -> Conversion {
        switch (tag) {
            case AttribScalarType::BYTE:
            case AttribScalarType::UNSIGNED_BYTE:
            case AttribScalarType::SHORT:
            case AttribScalarType::UNSIGNED_SHORT:
            case AttribScalarType::INT:
            case AttribScalarType::UNSIGNED_INT:
            case AttribScalarType::INT_2_10_10_10_REV:
            case AttribScalarType::UNSIGNED_INT_2_10_10_10_REV:
                return Conversion::INT;
            case AttribScalarType::HALF_FLOAT:
            case AttribScalarType::FLOAT:
            case AttribScalarType::FIXED:
            case AttribScalarType::UNSIGNED_INT_10F_11F_11F_REV:
                return Conversion::FLOAT;
            case AttribScalarType::DOUBLE:
                return Conversion::DOUBLE;
            default:
                unreachable();
        }
    }

    // Returns the size of a scalar type in bytes.
    consteval auto _scalar_size(AttribScalarType tag) -> size_t {
        switch (tag) {
            case AttribScalarType::BYTE:
                return sizeof(int8_t);
            case AttribScalarType::UNSIGNED_BYTE:
                return sizeof(uint8_t);
            case AttribScalarType::SHORT:
                return sizeof(int16_t);
            case AttribScalarType::UNSIGNED_SHORT:
                return sizeof(uint16_t);
            case AttribScalarType::INT:
                return sizeof(int32_t);
            case AttribScalarType::UNSIGNED_INT:
                return sizeof(uint32_t);
            case AttribScalarType::HALF_FLOAT:
                return 2;
            case AttribScalarType::FLOAT:
                return sizeof(float);
            case AttribScalarType::DOUBLE:
                return sizeof(double);
            case AttribScalarType::FIXED:
                return 4;
            case AttribScalarType::INT_2_10_10_10_REV:
                return 4;
            case AttribScalarType::UNSIGNED_INT_2_10_10_10_REV:
                return 4;
            case AttribScalarType::UNSIGNED_INT_10F_11F_11F_REV:
                return 4;
            default:
                unreachable();
        }
    }

    // Scalar attribute layout specifiers.
    export template <AttribScalarType element>
    struct Scalar {
        static constexpr auto base_type = _scalar_base_type(element);
        static constexpr auto elem_count = _scalar_packed_count(element);
        static constexpr auto mode = _scalar_mode(element);
        static constexpr auto size = _scalar_size(element);
    };

    // Vector attribute layout specifiers.
    export template <AttribScalarType element, size_t count>
    struct Vector {
        static constexpr auto base_type = _scalar_base_type(element);
        static constexpr auto elem_count = _scalar_packed_count(element) * static_cast<GLuint>(count);
        static constexpr auto mode = _scalar_mode(element);
        static constexpr auto size = _scalar_size(element) * count;
    };

    // Some layout aliases.
    export using Int8 = Scalar<AttribScalarType::BYTE>;
    export using UInt8 = Scalar<AttribScalarType::UNSIGNED_BYTE>;
    export using Int16 = Scalar<AttribScalarType::SHORT>;
    export using UInt16 = Scalar<AttribScalarType::UNSIGNED_SHORT>;
    export using Int = Scalar<AttribScalarType::INT>;
    export using UInt = Scalar<AttribScalarType::UNSIGNED_INT>;
    export using Half = Scalar<AttribScalarType::HALF_FLOAT>;
    export using Float = Scalar<AttribScalarType::FLOAT>;
    export using Double = Scalar<AttribScalarType::DOUBLE>;
    export using Fixed = Scalar<AttribScalarType::FIXED>;
    export using Vec4i_2_10_10_10 = Scalar<AttribScalarType::INT_2_10_10_10_REV>;
    export using Vec4u_2_10_10_10 = Scalar<AttribScalarType::UNSIGNED_INT_2_10_10_10_REV>;
    export using Vec3f_10_11_11 = Scalar<AttribScalarType::UNSIGNED_INT_10F_11F_11F_REV>;
    export using Vec2i8 = Vector<AttribScalarType::BYTE, 2>;
    export using Vec3i8 = Vector<AttribScalarType::BYTE, 3>;
    export using Vec4i8 = Vector<AttribScalarType::BYTE, 4>;
    export using Vec2u8 = Vector<AttribScalarType::UNSIGNED_BYTE, 2>;
    export using Vec3u8 = Vector<AttribScalarType::UNSIGNED_BYTE, 3>;
    export using Vec4u8 = Vector<AttribScalarType::UNSIGNED_BYTE, 4>;
    export using Vec2i16 = Vector<AttribScalarType::SHORT, 2>;
    export using Vec3i16 = Vector<AttribScalarType::SHORT, 3>;
    export using Vec4i16 = Vector<AttribScalarType::SHORT, 4>;
    export using Vec2u16 = Vector<AttribScalarType::UNSIGNED_SHORT, 2>;
    export using Vec3u16 = Vector<AttribScalarType::UNSIGNED_SHORT, 3>;
    export using Vec4u16 = Vector<AttribScalarType::UNSIGNED_SHORT, 4>;
    export using Vec2i = Vector<AttribScalarType::INT, 2>;
    export using Vec3i = Vector<AttribScalarType::INT, 3>;
    export using Vec4i = Vector<AttribScalarType::INT, 4>;
    export using Vec2u = Vector<AttribScalarType::UNSIGNED_INT, 2>;
    export using Vec3u = Vector<AttribScalarType::UNSIGNED_INT, 3>;
    export using Vec4u = Vector<AttribScalarType::UNSIGNED_INT, 4>;
    export using Vec2f = Vector<AttribScalarType::FLOAT, 2>;
    export using Vec3f = Vector<AttribScalarType::FLOAT, 3>;
    export using Vec4f = Vector<AttribScalarType::FLOAT, 4>;
    export using Vec2d = Vector<AttribScalarType::DOUBLE, 2>;
    export using Vec3d = Vector<AttribScalarType::DOUBLE, 3>;
    export using Vec4d = Vector<AttribScalarType::DOUBLE, 4>;

    // Wraps an attribute layout specifier with a semantic name.
    // These names are merely used to provide convenience in the API.
    export template <Layout T>
    struct Coord: T {};
    export template <Layout T>
    struct TexCoord: T {};
    export template <Layout T>
    struct Color: T {};
    export template <Layout T>
    struct Normal: T {};
    export template <Layout T>
    struct Material: T {};
}

// Provides information about using `T` as a common type.
// These are the types for which the GL attribute type layouts agree exactly with the C++ type layout.
template <Layout T>
struct _common_type {
    using cpp_type = std::monostate;
};

// Fundamental types.
template <>
struct _common_type<spec::Scalar<AttribScalarType::BYTE>> {
    using cpp_type = int8_t;
};

template <>
struct _common_type<spec::Scalar<AttribScalarType::UNSIGNED_BYTE>> {
    using cpp_type = uint8_t;
};

template <>
struct _common_type<spec::Scalar<AttribScalarType::SHORT>> {
    using cpp_type = int16_t;
};

template <>
struct _common_type<spec::Scalar<AttribScalarType::UNSIGNED_SHORT>> {
    using cpp_type = uint16_t;
};

template <>
struct _common_type<spec::Scalar<AttribScalarType::INT>> {
    using cpp_type = int32_t;
};

template <>
struct _common_type<spec::Scalar<AttribScalarType::UNSIGNED_INT>> {
    using cpp_type = uint32_t;
};

template <>
struct _common_type<spec::Scalar<AttribScalarType::FLOAT>> {
    using cpp_type = float;
};

template <>
struct _common_type<spec::Scalar<AttribScalarType::DOUBLE>> {
    using cpp_type = double;
};

// Interop with math types.
template <size_t N>
requires (2 <= N && N <= 4)
struct _common_type<spec::Vector<AttribScalarType::BYTE, N>> {
    using cpp_type = Vector<int8_t, N>;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct _common_type<spec::Vector<AttribScalarType::UNSIGNED_BYTE, N>> {
    using cpp_type = Vector<uint8_t, N>;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct _common_type<spec::Vector<AttribScalarType::SHORT, N>> {
    using cpp_type = Vector<int16_t, N>;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct _common_type<spec::Vector<AttribScalarType::UNSIGNED_SHORT, N>> {
    using cpp_type = Vector<uint16_t, N>;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct _common_type<spec::Vector<AttribScalarType::INT, N>> {
    using cpp_type = Vector<int32_t, N>;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct _common_type<spec::Vector<AttribScalarType::UNSIGNED_INT, N>> {
    using cpp_type = Vector<uint32_t, N>;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct _common_type<spec::Vector<AttribScalarType::FLOAT, N>> {
    using cpp_type = Vector<float, N>;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct _common_type<spec::Vector<AttribScalarType::DOUBLE, N>> {
    using cpp_type = Vector<double, N>;
};

// Transparency of semantic wrappers.
template <typename T>
struct _common_type<spec::Coord<T>>: _common_type<T> {};
template <typename T>
struct _common_type<spec::TexCoord<T>>: _common_type<T> {};
template <typename T>
struct _common_type<spec::Color<T>>: _common_type<T> {};
template <typename T>
struct _common_type<spec::Normal<T>>: _common_type<T> {};
template <typename T>
struct _common_type<spec::Material<T>>: _common_type<T> {};

// The result type of `interleave`.
export template <typename... T>
struct interleave_result {
    static constexpr auto count = sizeof...(T);
    size_t stride;
    std::array<size_t, count> offsets;
    std::array<size_t, count> sizes;
    std::array<std::tuple<GLenum, GLuint, Conversion>, count> args;
    using cpp_types = std::tuple<T...>;
};

// Interleaves the given attribute layout specifiers.
// Returns the offsets and sizes of each attribute, and an array of arguments for calling
// `glVertexAttribFormat()`.
export template <Layout... T>
struct interleave {
    static constexpr auto _sizes = std::array{T::size...};
    static_assert(((sizeof(typename _common_type<T>::cpp_type) <= T::size) && ...));
    static constexpr auto value = interleave_result<typename _common_type<T>::cpp_type...>{
        .stride = sum(_sizes),
        .offsets = prefix_sum(_sizes),
        .sizes = _sizes,
        .args = std::array{std::tuple<GLenum, GLuint, Conversion>{T::base_type, T::elem_count, T::mode}...},
    };
};

export template <Layout... T>
constexpr auto interleave_v = interleave<T...>::value;

}

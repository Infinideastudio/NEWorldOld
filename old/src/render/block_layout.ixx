module;

#include <glad/gl.h>

export module render:block_layout;
import std;
import types;
import debug;
import math;
import :types;

// # Block interface layouts
//
// The goal of this submodule is to take a `constexpr` description of a GLSL interface block
// (the description will be written in template parameter lists, since we require some types in
// the API to depend on this description) and return, for each query, the offset and size of the
// field with the requested name in the interface block.
//
// If there is some C++ type with the same layout as the field, we also return that type. This
// is useful for passing data through type-safe API. Currently, only scalars and vectors are
// supported. For matrices, arrays and structs, each element must be accessed individually.
// This is due to differences in the memory layout between the corresponding C++ and GLSL types.
//
// ## Example usage
//
// ```cpp
// using Interface = spec::Struct<spec::Field<
//     "vertices",
//     spec::Array<
//         spec::Struct<
//             spec::Field<"coord", spec::Vec3f>,
//             spec::Field<"color", spec::Vec3f>,
//             spec::Field<"normal", spec::Vec3i>,
//             spec::Field<
//                 "material",
//                 spec::Struct<
//                     spec::Field<"diffuse", spec::Vec3f>,
//                     spec::Field<"specular", spec::Vec3f>,
//                     spec::Field<"shininess", spec::Float>,
//                     spec::Field<"tex_coord", spec::Array<spec::Array<spec::Vec2f, 3>, 3>>>>>,
//         16>>>;
//
// constexpr auto result = locate_v<Interface, ".vertices[2].material.tex_coord[0][1]">;
// static_assert(result.offset == 544);
// static_assert(result.size == 8);
// static_assert(std::is_same_v<decltype(result)::cpp_type, Vec2f>);
// ```
namespace render::block_layout {
using types::FixedString;

// Tags for GLSL scalar types.
// See: https://www.khronos.org/opengl/wiki/Data_Type_(GLSL)
enum class GLSLScalarType { BOOL, INT, UNSIGNED_INT, FLOAT, DOUBLE };

// Concept for layout specifiers.
template <typename T>
concept Layout = requires {
    { T::align } -> std::convertible_to<size_t>;
    { T::size } -> std::convertible_to<size_t>;
};

// Concept for pairs of a name and a layout specifier.
template <typename T>
concept NameLayout = requires {
    { T::name };
    typename T::layout;
};

// Interface block layout specifiers.
namespace spec {

    // Returns the size of a scalar type in bytes.
    consteval auto _scalar_size(GLSLScalarType tag) -> size_t {
        switch (tag) {
            case GLSLScalarType::BOOL:
                return sizeof(bool);
            case GLSLScalarType::INT:
                return sizeof(int32_t);
            case GLSLScalarType::UNSIGNED_INT:
                return sizeof(uint32_t);
            case GLSLScalarType::FLOAT:
                return sizeof(float);
            case GLSLScalarType::DOUBLE:
                return sizeof(double);
            default:
                unreachable();
        }
    }

    // Returns the padded element count for a vector type.
    // In particular, std140 requires that `Vec3`s are padded to `Vec4`s, although `Vec2`s are not.
    consteval auto _vector_padded_count(size_t count) -> size_t {
        switch (count) {
            case 2:
                return 2;
            case 3:
                return 4;
            case 4:
                return 4;
            default:
                unreachable();
        }
    }

    // Returns the smallest multiple of `m` that is greater than or equal to `n`.
    consteval auto _round_up(size_t n, size_t m) -> size_t {
        auto remainder = n % m;
        return (remainder == 0) ? n : n + m - remainder;
    }

    // Returns `size` rounded up to a multiple of the size of a `Vec4f`.
    consteval auto _round_up_to_vec4f(size_t size) -> size_t {
        return _round_up(size, 4 * sizeof(float));
    }

    // Returns the largest alignment value among the fields, rounded up to a multiple of the size of a `Vec4f`.
    template <Layout... fields>
    consteval auto _struct_align() -> size_t {
        return _round_up_to_vec4f(std::max({1uz, fields::align...}));
    }

    // Layout rules for structures.
    // The structure alignment should be given in the `align` parameter.
    // The structure size is returned as the last element of the array.
    template <size_t align, Layout... fields>
    consteval auto _struct_field_offsets() -> std::array<size_t, sizeof...(fields) + 1> {
        constexpr auto N = sizeof...(fields);
        constexpr auto aligns = std::array<size_t, N>{fields::align...};
        constexpr auto sizes = std::array<size_t, N>{fields::size...};
        auto field_offsets = std::array<size_t, N + 1>{};
        auto offset = 0uz;
        for (auto i = 0uz; i < N; i++) {
            offset = _round_up(offset, aligns[i]);
            field_offsets[i] = offset;
            offset += sizes[i];
        }
        field_offsets[N] = _round_up(offset, align);
        return field_offsets;
    }

    // Interface block scalar layout specifiers.
    export template <GLSLScalarType element>
    struct Scalar {
        static constexpr auto align = _scalar_size(element);
        static constexpr auto size = _scalar_size(element);
    };

    // Interface block vector layout specifiers.
    export template <GLSLScalarType element, size_t count>
    struct Vector {
        static constexpr auto align = _scalar_size(element) * _vector_padded_count(count);
        static constexpr auto size = _scalar_size(element) * count;
    };

    // Interface block array layout specifiers.
    // Layout rules for arrays agree with that for structures.
    //
    // * In particular, for arrays of vectors, we have `align == stride` as expected by std140.
    // * In particular, for arrays of matrices (which are themselves arrays of vectors), we have the
    //   outer array `align` equals the inner array `align`, and therefore the outer array `stride`
    //   equals the inner array `size`, which is equivalent to the std140 wording.
    export template <Layout element, size_t count>
    struct Array {
        static constexpr auto align = _round_up_to_vec4f(element::align);
        static constexpr auto stride = _round_up(element::size, align);
        static constexpr auto size = stride * count;
    };

    // Interface block structure layout specifiers.
    export template <NameLayout... fields>
    struct Struct {
        static constexpr auto count = sizeof...(fields);
        static constexpr auto align = _struct_align<typename fields::layout...>();
        static constexpr auto field_offsets = _struct_field_offsets<align, typename fields::layout...>();
        static constexpr auto size = field_offsets.back();
    };

    // Interface block structure field specifiers.
    export template <FixedString field_name, Layout field_layout>
    struct Field {
        static constexpr auto name = field_name;
        using layout = field_layout;
    };

    // Some layout aliases.
    export using Bool = Scalar<GLSLScalarType::BOOL>;
    export using Int = Scalar<GLSLScalarType::INT>;
    export using UInt = Scalar<GLSLScalarType::UNSIGNED_INT>;
    export using Float = Scalar<GLSLScalarType::FLOAT>;
    export using Double = Scalar<GLSLScalarType::DOUBLE>;
    export using Vec2b = Vector<GLSLScalarType::BOOL, 2>;
    export using Vec3b = Vector<GLSLScalarType::BOOL, 3>;
    export using Vec4b = Vector<GLSLScalarType::BOOL, 4>;
    export using Vec2i = Vector<GLSLScalarType::INT, 2>;
    export using Vec3i = Vector<GLSLScalarType::INT, 3>;
    export using Vec4i = Vector<GLSLScalarType::INT, 4>;
    export using Vec2u = Vector<GLSLScalarType::UNSIGNED_INT, 2>;
    export using Vec3u = Vector<GLSLScalarType::UNSIGNED_INT, 3>;
    export using Vec4u = Vector<GLSLScalarType::UNSIGNED_INT, 4>;
    export using Vec2f = Vector<GLSLScalarType::FLOAT, 2>;
    export using Vec3f = Vector<GLSLScalarType::FLOAT, 3>;
    export using Vec4f = Vector<GLSLScalarType::FLOAT, 4>;
    export using Vec2d = Vector<GLSLScalarType::DOUBLE, 2>;
    export using Vec3d = Vector<GLSLScalarType::DOUBLE, 3>;
    export using Vec4d = Vector<GLSLScalarType::DOUBLE, 4>;
    export using Mat2f = Array<Vec2f, 2>;
    export using Mat2x3f = Array<Vec3f, 2>;
    export using Mat2x4f = Array<Vec4f, 2>;
    export using Mat3x2f = Array<Vec2f, 3>;
    export using Mat3f = Array<Vec3f, 3>;
    export using Mat3x4f = Array<Vec4f, 3>;
    export using Mat4x2f = Array<Vec2f, 4>;
    export using Mat4x3f = Array<Vec3f, 4>;
    export using Mat4f = Array<Vec4f, 4>;
}

// Provides information about using `T` as a common type.
// These are the types for which the GLSL type layouts agree exactly with the C++ type layout.
template <Layout T>
struct _common_type {
    using cpp_type = std::monostate;
};

// Fundamental types.
template <>
struct _common_type<spec::Scalar<GLSLScalarType::BOOL>> {
    using cpp_type = bool;
};

template <>
struct _common_type<spec::Scalar<GLSLScalarType::INT>> {
    using cpp_type = int32_t;
};

template <>
struct _common_type<spec::Scalar<GLSLScalarType::UNSIGNED_INT>> {
    using cpp_type = uint32_t;
};

template <>
struct _common_type<spec::Scalar<GLSLScalarType::FLOAT>> {
    using cpp_type = float;
};

template <>
struct _common_type<spec::Scalar<GLSLScalarType::DOUBLE>> {
    using cpp_type = double;
};

// Interop with math types.
template <size_t N>
requires (2 <= N && N <= 4)
struct _common_type<spec::Vector<GLSLScalarType::BOOL, N>> {
    using cpp_type = Vector<bool, N>;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct _common_type<spec::Vector<GLSLScalarType::INT, N>> {
    using cpp_type = Vector<int32_t, N>;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct _common_type<spec::Vector<GLSLScalarType::UNSIGNED_INT, N>> {
    using cpp_type = Vector<uint32_t, N>;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct _common_type<spec::Vector<GLSLScalarType::FLOAT, N>> {
    using cpp_type = Vector<float, N>;
};

template <size_t N>
requires (2 <= N && N <= 4)
struct _common_type<spec::Vector<GLSLScalarType::DOUBLE, N>> {
    using cpp_type = Vector<double, N>;
};

// The result type of `locate`.
export template <typename T = std::monostate>
struct locate_result {
    bool exists;
    size_t offset;
    size_t size;
    using cpp_type = T;
};

// Locates the given name in the given layout.
export template <Layout T, FixedString name>
struct locate {
    static constexpr auto value = locate_result{};
};

template <Layout element, size_t count, FixedString name>
consteval auto _locate_array_element(size_t stride) {
    constexpr auto s = std::string_view(name.chars.data());
    if constexpr (s[0] != '[') {
        return locate_result{};
    } else {
        constexpr auto i = s.find(']', 1);
        if constexpr (i == std::string_view::npos) {
            return locate_result{};
        } else {
            constexpr auto index = stouz(s.substr(1, i - 1));
            if constexpr (index >= count) {
                return locate_result{};
            } else {
                constexpr auto remaining = name.template suffix<i + 1>();
                auto res = locate<element, remaining>::value;
                if (res.exists) {
                    res.offset += stride * index;
                }
                return res;
            }
        }
    }
}

template <FixedString name, NameLayout... fields>
consteval auto _locate_struct_field(std::array<size_t, sizeof...(fields) + 1> const& field_offsets) {
    constexpr auto N = sizeof...(fields);
    constexpr auto s = std::string_view(name.chars.data());
    if constexpr (s[0] != '.') {
        return locate_result{};
    } else {
        constexpr auto i = std::min({s.find('.', 1), s.find('[', 1), s.size()});
        constexpr auto field_name = s.substr(1, i - 1);
        constexpr auto field_name_matches =
            std::array<bool, N>{std::string_view(fields::name.chars.data()) == field_name...};
        constexpr auto index = true_index(field_name_matches);
        if constexpr (index == N) {
            return locate_result{};
        } else {
            constexpr auto remaining = name.template suffix<i>();
            auto res = locate<typename std::tuple_element_t<index, std::tuple<fields...>>::layout, remaining>::value;
            if (res.exists) {
                res.offset += field_offsets[index];
            }
            return res;
        }
    }
}

// If the remaining name is empty, we have already located it.
template <Layout T>
struct locate<T, ""> {
    static_assert(sizeof(typename _common_type<T>::cpp_type) <= T::size);
    static constexpr auto value = locate_result<typename _common_type<T>::cpp_type>{
        .exists = true,
        .offset = 0uz,
        .size = T::size,
    };
};

// Locates the given name in the given array layout.
template <Layout element, size_t count, FixedString name>
struct locate<spec::Array<element, count>, name> {
    static constexpr auto value = _locate_array_element<element, count, name>(spec::Array<element, count>::stride);
};

// Locates the given name in the given struct layout.
template <NameLayout... fields, FixedString name>
struct locate<spec::Struct<fields...>, name> {
    static constexpr auto value = _locate_struct_field<name, fields...>(spec::Struct<fields...>::field_offsets);
};

export template <Layout T, FixedString name>
constexpr auto locate_v = locate<T, name>::value;

}

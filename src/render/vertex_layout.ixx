module;

#include <glad/gl.h>

export module render:vertex_layout;
import std;
import types;
import debug;
import math;

namespace render {

// Provides information about using `T` as a vertex attribute.
// Specializations should guarantee that the defined GL type layout matches the actual C++ memory layout of T.
// This gets a snake-case name since it is more like a *type-level function* than an actual class.
export template <typename T>
requires std::is_standard_layout_v<T> && std::is_default_constructible_v<T> && std::is_trivially_copyable_v<T>
struct vertex_attrib_type {};

template <>
struct vertex_attrib_type<int8_t> {
    static constexpr auto base_type = GL_BYTE;
    static constexpr auto elem_count = size_t{1};
};

template <>
struct vertex_attrib_type<uint8_t> {
    static constexpr auto base_type = GL_UNSIGNED_BYTE;
    static constexpr auto elem_count = size_t{1};
};

template <>
struct vertex_attrib_type<int16_t> {
    static constexpr auto base_type = GL_SHORT;
    static constexpr auto elem_count = size_t{1};
};

template <>
struct vertex_attrib_type<uint16_t> {
    static constexpr auto base_type = GL_UNSIGNED_SHORT;
    static constexpr auto elem_count = size_t{1};
};

template <>
struct vertex_attrib_type<int32_t> {
    static constexpr auto base_type = GL_INT;
    static constexpr auto elem_count = size_t{1};
};

template <>
struct vertex_attrib_type<uint32_t> {
    static constexpr auto base_type = GL_UNSIGNED_INT;
    static constexpr auto elem_count = size_t{1};
};

template <>
struct vertex_attrib_type<float> {
    static constexpr auto base_type = GL_FLOAT;
    static constexpr auto elem_count = size_t{1};
};

template <>
struct vertex_attrib_type<double> {
    static constexpr auto base_type = GL_DOUBLE;
    static constexpr auto elem_count = size_t{1};
};

// Interop with math types.
template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type<Vector<int8_t, N>> {
    static constexpr auto base_type = GL_BYTE;
    static constexpr auto elem_count = N;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type<Vector<uint8_t, N>> {
    static constexpr auto base_type = GL_UNSIGNED_BYTE;
    static constexpr auto elem_count = N;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type<Vector<int16_t, N>> {
    static constexpr auto base_type = GL_SHORT;
    static constexpr auto elem_count = N;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type<Vector<uint16_t, N>> {
    static constexpr auto base_type = GL_UNSIGNED_SHORT;
    static constexpr auto elem_count = N;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type<Vector<int32_t, N>> {
    static constexpr auto base_type = GL_INT;
    static constexpr auto elem_count = N;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type<Vector<uint32_t, N>> {
    static constexpr auto base_type = GL_UNSIGNED_INT;
    static constexpr auto elem_count = N;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type<Vector<float, N>> {
    static constexpr auto base_type = GL_FLOAT;
    static constexpr auto elem_count = N;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type<Vector<double, N>> {
    static constexpr auto base_type = GL_DOUBLE;
    static constexpr auto elem_count = N;
};

// Tests if `vertex_attrib_type` has the necessary specialization for `T`.
export template <typename T>
concept is_vertex_attrib_type = requires (T t) {
    // The base GL type name.
    // Must be one of:
    //     GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT, GL_INT, GL_UNSIGNED_INT,
    //     GL_HALF_FLOAT, GL_FLOAT, GL_DOUBLE, GL_FIXED, GL_INT_2_10_10_10_REV,
    //     GL_UNSIGNED_INT_2_10_10_10_REV, GL_UNSIGNED_INT_10F_11F_11F_REV.
    { vertex_attrib_type<T>::base_type } -> std::convertible_to<GLenum>;

    // Number of base components.
    // Must be 1, 2, 3 or 4.
    { vertex_attrib_type<T>::elem_count } -> std::convertible_to<GLint>;
};

// Returns if the GL type name should be passed as integers to shaders.
constexpr auto use_vertex_attrib_i_pointer(GLenum type) -> bool {
    return type == GL_BYTE || type == GL_UNSIGNED_BYTE || type == GL_SHORT || type == GL_UNSIGNED_SHORT
        || type == GL_INT || type == GL_UNSIGNED_INT;
}

// Returns if the GL type name should be passed as floats to shaders.
constexpr auto use_vertex_attrib_pointer(GLenum type) -> bool {
    return type == GL_HALF_FLOAT || type == GL_FLOAT || type == GL_FIXED || type == GL_INT_2_10_10_10_REV
        || type == GL_UNSIGNED_INT_2_10_10_10_REV || type == GL_UNSIGNED_INT_10F_11F_11F_REV;
}

// Returns if the GL type name should be passed as doubles to shaders.
constexpr auto use_vertex_attrib_l_pointer(GLenum type) -> bool {
    return type == GL_DOUBLE;
}

// Transparently wraps a vertex attribute type with a semantic name.
// These names are merely used to provide convenience in the API.
export template <typename T>
requires is_vertex_attrib_type<T>
struct Coord {
    T value;

    Coord() = default;
    Coord(T value):
        value(value) {}
};

template <typename T>
requires is_vertex_attrib_type<T>
struct vertex_attrib_type<Coord<T>>: vertex_attrib_type<T> {};

export template <typename T>
requires is_vertex_attrib_type<T>
struct TexCoord {
    T value;

    TexCoord() = default;
    TexCoord(T value):
        value(value) {}
};

template <typename T>
requires is_vertex_attrib_type<T>
struct vertex_attrib_type<TexCoord<T>>: vertex_attrib_type<T> {};

export template <typename T>
requires is_vertex_attrib_type<T>
struct Color {
    T value;

    Color() = default;
    Color(T value):
        value(value) {}
};

template <typename T>
requires is_vertex_attrib_type<T>
struct vertex_attrib_type<Color<T>>: vertex_attrib_type<T> {};

export template <typename T>
requires is_vertex_attrib_type<T>
struct Normal {
    T value;

    Normal() = default;
    Normal(T value):
        value(value) {}
};

template <typename T>
requires is_vertex_attrib_type<T>
struct vertex_attrib_type<Normal<T>>: vertex_attrib_type<T> {};

export template <typename T>
requires is_vertex_attrib_type<T>
struct Material {
    T value;

    Material() = default;
    Material(T value):
        value(value) {}
};

template <typename T>
requires is_vertex_attrib_type<T>
struct vertex_attrib_type<Material<T>>: vertex_attrib_type<T> {};

// Returns the sum of all elements in the array.
template <size_t N>
constexpr auto _sum(std::array<size_t, N> a) -> size_t {
    return std::accumulate(a.begin(), a.end(), 0);
}

// Returns the prefix sum of all elements in the array.
template <size_t N>
constexpr auto _prefix_sum(std::array<size_t, N> a) -> std::array<size_t, N> {
    auto res = std::array<size_t, N>{};
    auto sum = 0uz;
    for (auto i = 0uz; i < N; i++) {
        res[i] = sum;
        sum += a[i];
    }
    return res;
}

// Returns the I-th element in list <T...>. Switch to C++26 pack indexing once available.
template <size_t I, typename... T>
struct _choose {};

template <size_t I, typename T, typename... U>
struct _choose<I, T, U...>: _choose<I - 1, U...> {};

template <typename T, typename... U>
struct _choose<0, T, U...> {
    using type = T;
};

// A vertex in interleaved vertex layouts, with type-erased content.
// The attributes are stored in the order they appear in the template parameter list.
export template <typename... T>
requires (is_vertex_attrib_type<T> && ...)
class Vertex {
public:
    static constexpr auto ATTRIB_BASE_TYPES = std::array{static_cast<GLenum>(vertex_attrib_type<T>::base_type)...};
    static constexpr auto ATTRIB_ELEM_COUNTS = std::array{static_cast<GLint>(vertex_attrib_type<T>::elem_count)...};
    static constexpr auto ATTRIB_SIZES = std::array{sizeof(T)...};
    static constexpr auto ATTRIB_OFFSETS = _prefix_sum(ATTRIB_SIZES);
    static constexpr auto VERTEX_SIZE = _sum(ATTRIB_SIZES);

    template <size_t I>
    using Attrib = typename _choose<I, T...>::type;

    Vertex() = default;

    auto data() const -> std::span<std::byte const> {
        return std::span(_data);
    }

    auto size() const -> size_t {
        return _data.size();
    }

    template <size_t I>
    auto attrib() const -> Attrib<I> {
        auto attr = Attrib<I>();
        static_assert(sizeof(attr) == ATTRIB_SIZES[I]);
        auto bytes = std::as_writable_bytes(std::span(&attr, 1));
        auto offset = ATTRIB_OFFSETS[I];
        std::copy(_data.begin() + offset, _data.begin() + offset + ATTRIB_SIZES[I], bytes.begin());
        return attr;
    }

    template <size_t I>
    void set_attrib(Attrib<I> attr) {
        static_assert(sizeof(attr) == ATTRIB_SIZES[I]);
        auto bytes = std::as_bytes(std::span(&attr, 1));
        auto offset = ATTRIB_OFFSETS[I];
        std::copy(bytes.begin(), bytes.end(), _data.begin() + offset);
    }

private:
    std::array<std::byte, VERTEX_SIZE> _data = {};
};

// Note that `Vertex` has the alignment of 1 byte, and the size of all its attribute sizes summed.
// A contiguous array of `Vertex` will be tightly packed.
static_assert(alignof(Vertex<int32_t, Vec3f, float>) == 1);
static_assert(sizeof(Vertex<int32_t, Vec3f, float>) == sizeof(int32_t) + sizeof(Vec3f) + sizeof(float));

// Finds the first element in the list <T...> that is wrapped by a semantic wrapper W.
template <size_t I, template <typename> typename W, typename... T>
struct _find {};

template <size_t I, template <typename> typename W, typename T, typename... U>
struct _find<I, W, T, U...>: _find<I + 1, W, U...> {};

template <size_t I, template <typename> typename W, typename T, typename... U>
struct _find<I, W, W<T>, U...> {
    using type = T;
    static constexpr auto index = I;
};

// A vertex array on the CPU side.
//
// Example usage:
//
// ```
// auto builder = VertexArrayBuilder<int32_t, Vec3f, float>();
// builder.set_attrib<0>(1);
// builder.set_attrib<1>({1.0f, 2.0f, 3.0f});
// builder.set_attrib<2>(0.5f);
// builder.make_vertex();
// ```
//
// Example with semantic wrappers:
//
// ```
// auto builder = VertexArrayBuilder<Coord<Vec3f>, TexCoord<Vec2f>, Color<Vec4i>>();
// builder.color({255, 0, 0, 255});
// builder.tex_coord({0.5f, 0.5f});
// builder.coord({0.0f, 0.0f, 0.0f});
// builder.coord({1.0f, 0.0f, 0.0f});
// builder.coord({1.0f, 1.0f, 0.0f});
// ```
export template <typename... T>
requires (is_vertex_attrib_type<T> && ...)
class VertexArrayBuilder {
public:
    template <size_t I>
    using Attrib = _choose<I, T...>::type;

    using CoordAttrib = _find<0, Coord, T...>;
    using TexCoordAttrib = _find<0, TexCoord, T...>;
    using ColorAttrib = _find<0, Color, T...>;
    using NormalAttrib = _find<0, Normal, T...>;
    using MaterialAttrib = _find<0, Material, T...>;

    VertexArrayBuilder() = default;

    auto vertices() const -> std::vector<Vertex<T...>> const& {
        return _vertices;
    }

    void make_vertex() {
        _vertices.emplace_back(_vertex);
    }

    template <size_t I>
    void set_attrib(Attrib<I> attr, bool make_vertex = false) {
        _vertex.template set_attrib<I>(attr);
        if (make_vertex) {
            _vertices.emplace_back(_vertex);
        }
    }

    // Some convenience functions wrapping `set_attrib()`.
    void coord(CoordAttrib::type attr, bool make_vertex = true) {
        set_attrib<CoordAttrib::index>(attr, make_vertex);
    }

    void tex_coord(TexCoordAttrib::type attr, bool make_vertex = false) {
        set_attrib<TexCoordAttrib::index>(attr, make_vertex);
    }

    void color(ColorAttrib::type attr, bool make_vertex = false) {
        set_attrib<ColorAttrib::index>(attr, make_vertex);
    }

    void normal(NormalAttrib::type attr, bool make_vertex = false) {
        set_attrib<NormalAttrib::index>(attr, make_vertex);
    }

    void material(MaterialAttrib::type attr, bool make_vertex = false) {
        set_attrib<MaterialAttrib::index>(attr, make_vertex);
    }

private:
    Vertex<T...> _vertex;
    std::vector<Vertex<T...>> _vertices;
};

}

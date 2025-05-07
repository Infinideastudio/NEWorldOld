module;

#include <glad/gl.h>

export module render:vertex_layout;
import std;
import types;
import debug;
import math;

namespace render {

// Possible type conversion modes for vertex attributes.
export enum class VertexAttribMode { INTEGER, FLOAT, FLOAT_NORMALIZE, DOUBLE };

// Provides information about using `T` as a vertex attribute.
// This gets a snake-case name since it is more like a *type-level function* than an actual class.
export template <typename T>
requires std::is_standard_layout_v<T> && std::is_default_constructible_v<T> && std::is_trivially_copyable_v<T>
struct vertex_attrib_type_info {};

// Tests if `vertex_attrib_type_info` has the necessary specialization for `T`.
// Specializations should guarantee that the defined GL type layout matches the actual C++ memory layout of T.
export template <typename T>
concept vertex_attrib_type = requires (T t) {
    // The base GL type name.
    // Must be one of:
    //     GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT, GL_INT, GL_UNSIGNED_INT,
    //     GL_HALF_FLOAT, GL_FLOAT, GL_DOUBLE, GL_FIXED, GL_INT_2_10_10_10_REV,
    //     GL_UNSIGNED_INT_2_10_10_10_REV, GL_UNSIGNED_INT_10F_11F_11F_REV.
    { vertex_attrib_type_info<T>::base_type } -> std::convertible_to<GLenum>;

    // Number of base components.
    // Must be 1, 2, 3 or 4.
    { vertex_attrib_type_info<T>::elem_count } -> std::convertible_to<GLint>;

    // Type conversion mode.
    // Must be compatible with the base type.
    // See: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glVertexAttribPointer.xhtml
    { vertex_attrib_type_info<T>::mode } -> std::convertible_to<VertexAttribMode>;
};

// Fundamental types that can be passed directly to shaders.
template <>
struct vertex_attrib_type_info<int8_t> {
    static constexpr auto base_type = GL_BYTE;
    static constexpr auto elem_count = size_t{1};
    static constexpr auto mode = VertexAttribMode::INTEGER;
};

template <>
struct vertex_attrib_type_info<uint8_t> {
    static constexpr auto base_type = GL_UNSIGNED_BYTE;
    static constexpr auto elem_count = size_t{1};
    static constexpr auto mode = VertexAttribMode::INTEGER;
};

template <>
struct vertex_attrib_type_info<int16_t> {
    static constexpr auto base_type = GL_SHORT;
    static constexpr auto elem_count = size_t{1};
    static constexpr auto mode = VertexAttribMode::INTEGER;
};

template <>
struct vertex_attrib_type_info<uint16_t> {
    static constexpr auto base_type = GL_UNSIGNED_SHORT;
    static constexpr auto elem_count = size_t{1};
    static constexpr auto mode = VertexAttribMode::INTEGER;
};

template <>
struct vertex_attrib_type_info<int32_t> {
    static constexpr auto base_type = GL_INT;
    static constexpr auto elem_count = size_t{1};
    static constexpr auto mode = VertexAttribMode::INTEGER;
};

template <>
struct vertex_attrib_type_info<uint32_t> {
    static constexpr auto base_type = GL_UNSIGNED_INT;
    static constexpr auto elem_count = size_t{1};
    static constexpr auto mode = VertexAttribMode::INTEGER;
};

template <>
struct vertex_attrib_type_info<float> {
    static constexpr auto base_type = GL_FLOAT;
    static constexpr auto elem_count = size_t{1};
    static constexpr auto mode = VertexAttribMode::FLOAT;
};

template <>
struct vertex_attrib_type_info<double> {
    static constexpr auto base_type = GL_DOUBLE;
    static constexpr auto elem_count = size_t{1};
    static constexpr auto mode = VertexAttribMode::DOUBLE;
};

// Interop with math types.
template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type_info<Vector<int8_t, N>> {
    static constexpr auto base_type = GL_BYTE;
    static constexpr auto elem_count = N;
    static constexpr auto mode = VertexAttribMode::INTEGER;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type_info<Vector<uint8_t, N>> {
    static constexpr auto base_type = GL_UNSIGNED_BYTE;
    static constexpr auto elem_count = N;
    static constexpr auto mode = VertexAttribMode::INTEGER;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type_info<Vector<int16_t, N>> {
    static constexpr auto base_type = GL_SHORT;
    static constexpr auto elem_count = N;
    static constexpr auto mode = VertexAttribMode::INTEGER;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type_info<Vector<uint16_t, N>> {
    static constexpr auto base_type = GL_UNSIGNED_SHORT;
    static constexpr auto elem_count = N;
    static constexpr auto mode = VertexAttribMode::INTEGER;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type_info<Vector<int32_t, N>> {
    static constexpr auto base_type = GL_INT;
    static constexpr auto elem_count = N;
    static constexpr auto mode = VertexAttribMode::INTEGER;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type_info<Vector<uint32_t, N>> {
    static constexpr auto base_type = GL_UNSIGNED_INT;
    static constexpr auto elem_count = N;
    static constexpr auto mode = VertexAttribMode::INTEGER;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type_info<Vector<float, N>> {
    static constexpr auto base_type = GL_FLOAT;
    static constexpr auto elem_count = N;
    static constexpr auto mode = VertexAttribMode::FLOAT;
};

template <size_t N>
requires (1 <= N && N <= 4)
struct vertex_attrib_type_info<Vector<double, N>> {
    static constexpr auto base_type = GL_DOUBLE;
    static constexpr auto elem_count = N;
    static constexpr auto mode = VertexAttribMode::DOUBLE;
};

// Transparently wraps a vertex attribute type with a semantic name.
// These names are merely used to provide convenience in the API.
export template <typename T>
requires vertex_attrib_type<T>
struct Coord {
    T value;

    Coord() = default;
    Coord(T value):
        value(value) {}
};

template <typename T>
struct vertex_attrib_type_info<Coord<T>>: vertex_attrib_type_info<T> {};

export template <typename T>
requires vertex_attrib_type<T>
struct TexCoord {
    T value;

    TexCoord() = default;
    TexCoord(T value):
        value(value) {}
};

template <typename T>
struct vertex_attrib_type_info<TexCoord<T>>: vertex_attrib_type_info<T> {};

export template <typename T>
requires vertex_attrib_type<T>
struct Color {
    T value;

    Color() = default;
    Color(T value):
        value(value) {}
};

template <typename T>
struct vertex_attrib_type_info<Color<T>>: vertex_attrib_type_info<T> {};

export template <typename T>
requires vertex_attrib_type<T>
struct Normal {
    T value;

    Normal() = default;
    Normal(T value):
        value(value) {}
};

template <typename T>
struct vertex_attrib_type_info<Normal<T>>: vertex_attrib_type_info<T> {};

export template <typename T>
requires vertex_attrib_type<T>
struct Material {
    T value;

    Material() = default;
    Material(T value):
        value(value) {}
};

template <typename T>
struct vertex_attrib_type_info<Material<T>>: vertex_attrib_type_info<T> {};

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
requires (vertex_attrib_type<T> && ...)
class Vertex {
public:
    static constexpr auto ATTRIB_BASE_TYPES = std::array{static_cast<GLenum>(vertex_attrib_type_info<T>::base_type)...};
    static constexpr auto ATTRIB_ELEM_COUNTS =
        std::array{static_cast<GLint>(vertex_attrib_type_info<T>::elem_count)...};
    static constexpr auto ATTRIB_MODES = std::array{static_cast<VertexAttribMode>(vertex_attrib_type_info<T>::mode)...};
    static constexpr auto ATTRIB_SIZES = std::array{sizeof(T)...};
    static constexpr auto ATTRIB_OFFSETS = _prefix_sum(ATTRIB_SIZES);
    static constexpr auto VERTEX_SIZE = _sum(ATTRIB_SIZES);

    // `Attrib<i>::type` gives the type of the i-th attribute.
    template <size_t I>
    using Attrib = _choose<I, T...>;

    Vertex() = default;

    auto bytes() const -> std::array<std::byte, VERTEX_SIZE> const& {
        return _bytes;
    }

    auto size() const -> size_t {
        return _bytes.size();
    }

    template <size_t I>
    auto attrib() const -> Attrib<I>::type {
        auto attr = Attrib<I>::type();
        auto span = std::as_writable_bytes(std::span(&attr, 1));
        auto src = _bytes.begin() + ATTRIB_OFFSETS[I];
        std::copy(src, src + span.size(), span.begin());
        return attr;
    }

    template <size_t I>
    void set_attrib(Attrib<I>::type attr) {
        auto span = std::as_bytes(std::span(&attr, 1));
        auto dst = _bytes.begin() + ATTRIB_OFFSETS[I];
        std::copy(span.begin(), span.end(), dst);
    }

private:
    std::array<std::byte, VERTEX_SIZE> _bytes = {};
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

template <typename... T>
concept has_coord_attrib = requires { typename _find<0, Coord, T...>::type; };

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
requires (vertex_attrib_type<T> && ...)
class VertexArrayBuilder {
public:
    // `Attrib<i>::type` gives the type of the i-th attribute.
    template <size_t I>
    using Attrib = _choose<I, T...>;

    // These are only enabled if the corresponding semantic wrapper is found in `T`.
    // The additional template parameter `I = 0` delays instantiation to make SFINAE work.
    template <size_t I>
    using CoordAttrib = _find<I, Coord, T...>;
    template <size_t I>
    using TexCoordAttrib = _find<I, TexCoord, T...>;
    template <size_t I>
    using ColorAttrib = _find<I, Color, T...>;
    template <size_t I>
    using NormalAttrib = _find<I, Normal, T...>;
    template <size_t I>
    using MaterialAttrib = _find<I, Material, T...>;

    VertexArrayBuilder() = default;

    auto vertices() const -> std::vector<Vertex<T...>> const& {
        return _vertices;
    }

    auto size() const -> size_t {
        return _vertices.size();
    }

    void make_vertex() {
        _vertices.emplace_back(_vertex);
    }

    template <size_t I>
    void set_attrib(Attrib<I>::type attr, bool make_vertex = false) {
        _vertex.template set_attrib<I>(attr);
        if (make_vertex) {
            _vertices.emplace_back(_vertex);
        }
    }

    // Some convenience functions wrapping `set_attrib()`.
    // These are only enabled if the corresponding semantic wrapper is found in `T`.
    template <size_t I = 0, typename = CoordAttrib<I>::type>
    void coord(CoordAttrib<I>::type attr, bool make_vertex = true) {
        set_attrib<CoordAttrib<I>::index>(attr, make_vertex);
    }
    template <size_t I = 0, typename = TexCoordAttrib<I>::type>
    void tex_coord(TexCoordAttrib<I>::type attr, bool make_vertex = false) {
        set_attrib<TexCoordAttrib<I>::index>(attr, make_vertex);
    }
    template <size_t I = 0, typename = ColorAttrib<I>::type>
    void color(ColorAttrib<I>::type attr, bool make_vertex = false) {
        set_attrib<ColorAttrib<I>::index>(attr, make_vertex);
    }
    template <size_t I = 0, typename = NormalAttrib<I>::type>
    void normal(NormalAttrib<I>::type attr, bool make_vertex = false) {
        set_attrib<NormalAttrib<I>::index>(attr, make_vertex);
    }
    template <size_t I = 0, typename = MaterialAttrib<I>::type>
    void material(MaterialAttrib<I>::type attr, bool make_vertex = false) {
        set_attrib<MaterialAttrib<I>::index>(attr, make_vertex);
    }

private:
    Vertex<T...> _vertex;
    std::vector<Vertex<T...>> _vertices;
};

}

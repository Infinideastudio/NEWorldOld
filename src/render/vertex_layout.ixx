module;

#include <glad/gl.h>

export module render:vertex_layout;
import std;
import types;
import debug;
import math;
import :types;

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

// Vertex attribute type lists.
// The attributes are stored in the order they appear in the type list.
export template <typename... T>
requires (vertex_attrib_type<T> && ...)
class VertexLayout {
public:
    static constexpr auto ATTRIB_COUNT = sizeof...(T);
    static constexpr auto ATTRIB_BASE_TYPES = std::array{static_cast<GLenum>(vertex_attrib_type_info<T>::base_type)...};
    static constexpr auto ATTRIB_ELEM_COUNTS =
        std::array{static_cast<GLint>(vertex_attrib_type_info<T>::elem_count)...};
    static constexpr auto ATTRIB_MODES = std::array{static_cast<VertexAttribMode>(vertex_attrib_type_info<T>::mode)...};
    static constexpr auto ATTRIB_SIZES = std::array{sizeof(T)...};
    static constexpr auto ATTRIB_OFFSETS = prefix_sum(ATTRIB_SIZES);
    static constexpr auto VERTEX_SIZE = sum(ATTRIB_SIZES);
};

// A vertex on the CPU side. Stores type-erased attribute data.
export template <typename Layout>
class Vertex {};

template <typename... T>
class Vertex<VertexLayout<T...>> {
public:
    // `Layout` gives information of the specified vertex layout.
    using Layout = VertexLayout<T...>;

    // `Attrib<i>` gives the type of the i-th attribute.
    template <size_t I>
    using Attrib = std::tuple_element_t<I, std::tuple<T...>>;

    Vertex() = default;

    auto bytes() const -> std::array<std::byte, Layout::VERTEX_SIZE> const& {
        return _bytes;
    }

    auto size() const -> size_t {
        return _bytes.size();
    }

    template <size_t I>
    auto attrib() const -> Attrib<I> {
        auto attr = Attrib<I>();
        auto span = std::as_writable_bytes(std::span(&attr, 1));
        auto src = _bytes.begin() + Layout::ATTRIB_OFFSETS[I];
        std::copy(src, src + span.size(), span.begin());
        return attr;
    }

    template <size_t I>
    void set_attrib(Attrib<I> attr) {
        auto span = std::as_bytes(std::span(&attr, 1));
        auto dst = _bytes.begin() + Layout::ATTRIB_OFFSETS[I];
        std::copy(span.begin(), span.end(), dst);
    }

private:
    std::array<std::byte, Layout::VERTEX_SIZE> _bytes = {};
};

// Note that `Vertex` has the alignment of 1 byte, and the size of all its attribute sizes summed.
// A contiguous array of `Vertex` will be tightly packed.
static_assert(alignof(Vertex<VertexLayout<int32_t, Vec3f, float>>) == 1);
static_assert(sizeof(Vertex<VertexLayout<int32_t, Vec3f, float>>) == sizeof(int32_t) + sizeof(Vec3f) + sizeof(float));

// A vertex array on the CPU side.
//
// Example usage:
//
// ```
// using Layout = VertexLayout<int32_t, Vec3f, float>;
// auto builder = VertexArrayBuilder<Layout>();
// builder.set_attrib<0>(1);
// builder.set_attrib<1>({1.0f, 2.0f, 3.0f});
// builder.set_attrib<2>(0.5f);
// builder.make_vertex();
// ```
//
// Example with semantic wrappers:
//
// ```
// using Layout = VertexLayout<Coord<Vec3f>, TexCoord<Vec2f>, Color<Vec4i>>;
// auto builder = VertexArrayBuilder<Layout>();
// builder.color({255, 0, 0, 255});
// builder.tex_coord({0.5f, 0.5f});
// builder.coord({0.0f, 0.0f, 0.0f});
// builder.coord({1.0f, 0.0f, 0.0f});
// builder.coord({1.0f, 1.0f, 0.0f});
// ```
export template <typename Layout>
class VertexArrayBuilder {};

template <typename... T>
class VertexArrayBuilder<VertexLayout<T...>> {
public:
    // `Layout` gives information of the specified vertex layout.
    using Layout = VertexLayout<T...>;

    // `Attrib<i>` gives the type of the i-th attribute.
    template <size_t I>
    using Attrib = std::tuple_element_t<I, std::tuple<T...>>;

    // Semantic wrapper find results.
    using CoordAttrib = find_wrapper_t<Coord, T...>;
    using TexCoordAttrib = find_wrapper_t<TexCoord, T...>;
    using ColorAttrib = find_wrapper_t<Color, T...>;
    using NormalAttrib = find_wrapper_t<Normal, T...>;
    using MaterialAttrib = find_wrapper_t<Material, T...>;

    VertexArrayBuilder() = default;

    auto vertices() const -> std::vector<Vertex<Layout>> const& {
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
    // These are only enabled if the corresponding semantic wrapper is found in `T`.
    void coord(CoordAttrib::type attr, bool make_vertex = true) requires CoordAttrib::found
    {
        set_attrib<CoordAttrib::index>(attr, make_vertex);
    }
    void tex_coord(TexCoordAttrib::type attr, bool make_vertex = false) requires TexCoordAttrib::found
    {
        set_attrib<TexCoordAttrib::index>(attr, make_vertex);
    }
    void color(ColorAttrib::type attr, bool make_vertex = false) requires ColorAttrib::found
    {
        set_attrib<ColorAttrib::index>(attr, make_vertex);
    }
    void normal(NormalAttrib::type attr, bool make_vertex = false) requires NormalAttrib::found
    {
        set_attrib<NormalAttrib::index>(attr, make_vertex);
    }
    void material(MaterialAttrib::type attr, bool make_vertex = false) requires MaterialAttrib::found
    {
        set_attrib<MaterialAttrib::index>(attr, make_vertex);
    }

private:
    Vertex<Layout> _vertex;
    std::vector<Vertex<Layout>> _vertices;
};

// A vertex array together with an index array on the CPU side.
export template <typename Layout>
class VertexArrayIndexedBuilder {};

template <typename... T>
class VertexArrayIndexedBuilder<VertexLayout<T...>> {
public:
    // `Layout` gives information of the specified vertex layout.
    using Layout = VertexLayout<T...>;

    // `Attrib<i>` gives the type of the i-th attribute.
    template <size_t I>
    using Attrib = std::tuple_element_t<I, std::tuple<T...>>;

    // Semantic wrapper find results.
    using CoordAttrib = find_wrapper_t<Coord, T...>;
    using TexCoordAttrib = find_wrapper_t<TexCoord, T...>;
    using ColorAttrib = find_wrapper_t<Color, T...>;
    using NormalAttrib = find_wrapper_t<Normal, T...>;
    using MaterialAttrib = find_wrapper_t<Material, T...>;

    // Primitive restart index.
    // When cast to narrower types, this will become the fixed primitive restart indices
    // i.e. `0xFF` for `uint8_t` and `0xFFFF` for `uint16_t`.
    static constexpr auto PRIMITIVE_RESTART_INDEX = std::numeric_limits<uint32_t>::max();

    VertexArrayIndexedBuilder() = default;

    auto vertices() const -> std::vector<Vertex<Layout>> const& {
        return _vertices;
    }

    auto indices() const -> std::vector<uint32_t> const& {
        return _indices;
    }

    // Appends a new vertex with a new index to the arrays.
    void make_vertex() {
        assert(_vertices.size() < PRIMITIVE_RESTART_INDEX, "vertex array too large");
        _indices.push_back(static_cast<uint32_t>(_vertices.size()));
        _vertices.emplace_back(_vertex);
    }

    // Appends a new index referring to an existing vertex.
    // The last inserted vertex has relative index 0.
    void repeat_vertex(size_t relative) {
        assert(relative + 1 <= _vertices.size(), "index out of bounds");
        _indices.push_back(static_cast<uint32_t>(_vertices.size() - (relative + 1)));
    }

    // Appends the primitive restart index.
    void end_primitive() {
        _indices.push_back(PRIMITIVE_RESTART_INDEX);
    }

    template <size_t I>
    void set_attrib(Attrib<I> attr, bool make_vertex = false) {
        _vertex.template set_attrib<I>(attr);
        if (make_vertex) {
            assert(_vertices.size() < PRIMITIVE_RESTART_INDEX, "vertex array too large");
            _indices.push_back(static_cast<uint32_t>(_vertices.size()));
            _vertices.emplace_back(_vertex);
        }
    }

    // Some convenience functions wrapping `set_attrib()`.
    // These are only enabled if the corresponding semantic wrapper is found in `T`.
    void coord(CoordAttrib::type attr, bool make_vertex = true) requires CoordAttrib::found
    {
        set_attrib<CoordAttrib::index>(attr, make_vertex);
    }
    void tex_coord(TexCoordAttrib::type attr, bool make_vertex = false) requires TexCoordAttrib::found
    {
        set_attrib<TexCoordAttrib::index>(attr, make_vertex);
    }
    void color(ColorAttrib::type attr, bool make_vertex = false) requires ColorAttrib::found
    {
        set_attrib<ColorAttrib::index>(attr, make_vertex);
    }
    void normal(NormalAttrib::type attr, bool make_vertex = false) requires NormalAttrib::found
    {
        set_attrib<NormalAttrib::index>(attr, make_vertex);
    }
    void material(MaterialAttrib::type attr, bool make_vertex = false) requires MaterialAttrib::found
    {
        set_attrib<MaterialAttrib::index>(attr, make_vertex);
    }

private:
    Vertex<Layout> _vertex;
    std::vector<Vertex<Layout>> _vertices;
    std::vector<uint32_t> _indices;
};

}

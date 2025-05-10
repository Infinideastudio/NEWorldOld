module;

#include <glad/gl.h>

export module render:attrib_builder;
import std;
import types;
import debug;
import math;
import :types;
import :attrib_layout;

namespace render {
using namespace attrib_layout;

// A tuple of interleaved attributes on the CPU side.
export template <Layout... T>
class Vertex {
public:
    // Interleaving results.
    static constexpr auto interleaved = interleave_v<T...>;

    // `Attrib<i>` gives the type of the i-th attribute.
    template <size_t I>
    requires (I < sizeof...(T))
    using Attrib = std::tuple_element_t<I, typename decltype(interleaved)::cpp_types>;

    Vertex() = default;

    auto bytes() const -> std::array<std::byte, interleaved.stride> const& {
        return _bytes;
    }

    static constexpr auto size() -> size_t {
        return interleaved.size;
    }

    template <size_t I>
    auto get() const -> Attrib<I> {
        static_assert(sizeof(Attrib<I>) == interleaved.sizes[I]);
        auto attr = Attrib<I>();
        auto span = std::as_writable_bytes(std::span<Attrib<I>, 1>(&attr, 1));
        auto src = _bytes.begin() + interleaved.offsets[I];
        std::copy_n(src, span.size(), span.begin());
        return attr;
    }

    template <size_t I>
    void set(Attrib<I> attr) {
        static_assert(sizeof(Attrib<I>) == interleaved.sizes[I]);
        auto span = std::as_bytes(std::span<Attrib<I>, 1>(&attr, 1));
        auto dst = _bytes.begin() + interleaved.offsets[I];
        std::copy_n(span.begin(), span.size(), dst);
    }

private:
    std::array<std::byte, interleaved.stride> _bytes = {};
};

// Note that `Vertex` has the alignment of 1 byte, and the size of all its attribute sizes summed.
// A contiguous array of `Vertex` will be tightly packed.
namespace {
    using Example = Vertex<spec::Int, spec::Vec3f, spec::Float>;

    static_assert(alignof(Example) == 1);
    static_assert(sizeof(Example) == sizeof(int32_t) + sizeof(Vec3f) + sizeof(float));
}

// A vertex array on the CPU side.
//
// Example usage:
//
// ```
// namespace spec = attrib_layout::spec;
// auto builder = AttribBuilder<spec::Int, spec::Vec3f, spec::Float>();
// builder.set<0>(1);
// builder.set<1>({1.0f, 2.0f, 3.0f});
// builder.set<2>(0.5f);
// builder.make_vertex();
// ```
//
// Example with semantic wrappers:
//
// ```
// namespace spec = attrib_layout::spec;
// auto builder = AttribBuilder<spec::Coord<spec::Vec3f>, spec::TexCoord<spec::Vec2f>, spec::Color<spec::Vec4i>>();
// builder.color({255, 0, 0, 255});
// builder.tex_coord({0.5f, 0.5f});
// builder.coord({0.0f, 0.0f, 0.0f});
// builder.coord({1.0f, 0.0f, 0.0f});
// builder.coord({1.0f, 1.0f, 0.0f});
// ```
export template <Layout... T>
class AttribBuilder {
public:
    // Interleaving results.
    static constexpr auto interleaved = interleave_v<T...>;

    // Semantic wrapper find results.
    static constexpr auto coord_index = wrapped_index_v<spec::Coord, T...>;
    static constexpr auto tex_coord_index = wrapped_index_v<spec::TexCoord, T...>;
    static constexpr auto color_index = wrapped_index_v<spec::Color, T...>;
    static constexpr auto normal_index = wrapped_index_v<spec::Normal, T...>;
    static constexpr auto material_index = wrapped_index_v<spec::Material, T...>;

    // `Attrib<i>` gives the type of the i-th attribute.
    template <size_t I>
    requires (I < sizeof...(T))
    using Attrib = std::tuple_element_t<I, typename decltype(interleaved)::cpp_types>;

    AttribBuilder() = default;

    auto vertices() const -> std::vector<Vertex<T...>> const& {
        return _vertices;
    }

    void make_vertex() {
        _vertices.emplace_back(_vertex);
    }

    template <size_t I>
    void set(Attrib<I> attr, bool make_vertex = false) {
        _vertex.template set<I>(attr);
        if (make_vertex) {
            _vertices.emplace_back(_vertex);
        }
    }

    // Some convenience functions wrapping `set()`.
    // These are only enabled if the corresponding semantic wrapper is found in `T`.
    template <size_t I = coord_index>
    void coord(Attrib<I> attr) {
        set<I>(attr, true);
    }
    template <size_t I = tex_coord_index>
    void tex_coord(Attrib<I> attr) {
        set<I>(attr, false);
    }
    template <size_t I = color_index>
    void color(Attrib<I> attr) {
        set<I>(attr, false);
    }
    template <size_t I = normal_index>
    void normal(Attrib<I> attr) {
        set<I>(attr, false);
    }
    template <size_t I = material_index>
    void material(Attrib<I> attr) {
        set<I>(attr, false);
    }

private:
    Vertex<T...> _vertex;
    std::vector<Vertex<T...>> _vertices;
};

// A vertex array together with an index array on the CPU side.
export template <Layout... T>
class AttribIndexBuilder {
public:
    // Interleaving results.
    static constexpr auto interleaved = interleave_v<T...>;

    // Semantic wrapper find results.
    static constexpr auto coord_index = wrapped_index_v<spec::Coord, T...>;
    static constexpr auto tex_coord_index = wrapped_index_v<spec::TexCoord, T...>;
    static constexpr auto color_index = wrapped_index_v<spec::Color, T...>;
    static constexpr auto normal_index = wrapped_index_v<spec::Normal, T...>;
    static constexpr auto material_index = wrapped_index_v<spec::Material, T...>;

    // `Attrib<i>` gives the type of the i-th attribute.
    template <size_t I>
    requires (I < sizeof...(T))
    using Attrib = std::tuple_element_t<I, typename decltype(interleaved)::cpp_types>;

    // Primitive restart index.
    // When cast to narrower types, this will become the fixed primitive restart indices
    // i.e. `0xFF` for `uint8_t` and `0xFFFF` for `uint16_t`.
    static constexpr auto PRIMITIVE_RESTART_INDEX = std::numeric_limits<uint32_t>::max();

    AttribIndexBuilder() = default;

    auto vertices() const -> std::vector<Vertex<T...>> const& {
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
    void set(Attrib<I> attr, bool make_vertex = false) {
        _vertex.template set<I>(attr);
        if (make_vertex) {
            assert(_vertices.size() < PRIMITIVE_RESTART_INDEX, "vertex array too large");
            _indices.push_back(static_cast<uint32_t>(_vertices.size()));
            _vertices.emplace_back(_vertex);
        }
    }

    // Some convenience functions wrapping `set()`.
    // These are only enabled if the corresponding semantic wrapper is found in `T`.
    template <size_t I = coord_index>
    void coord(Attrib<I> attr) {
        set<I>(attr, true);
    }
    template <size_t I = tex_coord_index>
    void tex_coord(Attrib<I> attr) {
        set<I>(attr, false);
    }
    template <size_t I = color_index>
    void color(Attrib<I> attr) {
        set<I>(attr, false);
    }
    template <size_t I = normal_index>
    void normal(Attrib<I> attr) {
        set<I>(attr, false);
    }
    template <size_t I = material_index>
    void material(Attrib<I> attr) {
        set<I>(attr, false);
    }

private:
    Vertex<T...> _vertex;
    std::vector<Vertex<T...>> _vertices;
    std::vector<uint32_t> _indices;
};

}

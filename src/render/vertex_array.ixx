module;

#include <glad/gl.h>

export module render:vertex_array;
import std;
import types;
import debug;
import :types;
import :buffer;
import :attrib_layout;
import :attrib_builder;

namespace render {
using namespace attrib_layout;

// Helper function to delete a valid GL vertex array object.
void _vertex_array_deleter(GLuint handle) {
    glDeleteVertexArrays(1, &handle);
}

// Manages a GL vertex array object.
export class VertexArray {
public:
    // Possible primitive types per GL 4.3.
    enum class Primitive : GLenum {
        POINTS = GL_POINTS,
        LINE_STRIP = GL_LINE_STRIP,
        LINE_LOOP = GL_LINE_LOOP,
        LINES = GL_LINES,
        LINE_STRIP_ADJACENCY = GL_LINE_STRIP_ADJACENCY,
        LINES_ADJACENCY = GL_LINES_ADJACENCY,
        TRIANGLE_STRIP = GL_TRIANGLE_STRIP,
        TRIANGLE_FAN = GL_TRIANGLE_FAN,
        TRIANGLES = GL_TRIANGLES,
        TRIANGLE_STRIP_ADJACENCY = GL_TRIANGLE_STRIP_ADJACENCY,
        TRIANGLES_ADJACENCY = GL_TRIANGLES_ADJACENCY,
        PATCHES = GL_PATCHES,
    };

    // Possible index types per GL 4.3.
    enum class Index : GLenum {
        NONE = 0,
        UNSIGNED_BYTE = GL_UNSIGNED_BYTE,
        UNSIGNED_SHORT = GL_UNSIGNED_SHORT,
        UNSIGNED_INT = GL_UNSIGNED_INT,
    };

    // Constructs a `VertexArray` which owns nothing.
    VertexArray() noexcept = default;

    // Constructs a `VertexArray` which owns the given `handle`.
    // The `handle` must be either 0 or a valid GL vertex array object.
    // The `count` must not be more than the number of vertices in the object's storage.
    explicit VertexArray(
        GLuint handle,
        Primitive primitive,
        size_t count,
        size_t index_range,
        Index index_type,
        size_t index_offset
    ) noexcept:
        _handle(handle),
        _primitive(primitive),
        _count(count),
        _index_range(index_range),
        _index_type(index_type),
        _index_offset(index_offset) {}

    auto get() const noexcept -> GLuint {
        return _handle.get();
    }

    operator bool() const noexcept {
        return bool(_handle);
    }

    auto count() const noexcept -> size_t {
        return _count;
    }

    void bind() const {
        // Permissive of empty vertex arrays.
        if (*this) {
            glBindVertexArray(get());
        }
    }

    void render() const {
        // Permissive of empty vertex arrays.
        if (*this) {
            if (_index_type == Index::NONE) {
                glBindVertexArray(get());
                glDrawArrays(_primitive_to_gl_enum(_primitive), 0, static_cast<GLsizei>(_count));
            } else {
                glBindVertexArray(get());
                glPrimitiveRestartIndex(_fixed_restart_index(_index_type));
                glDrawRangeElements(
                    _primitive_to_gl_enum(_primitive),
                    static_cast<GLuint>(0),
                    static_cast<GLuint>(_index_range - 1),
                    static_cast<GLsizei>(_count),
                    _index_type_to_gl_enum(_index_type),
                    reinterpret_cast<void const*>(_index_offset)
                );
            }
        }
    }

    // Constructs a vertex array object from a vertex array builder.
    // Returns the constructed vertex array, as well as its underlying buffer object.
    //
    // The binding locations of vertex attributes are determined by the order in which they occur
    // in the template arguments. The first attribute is bound to location 0, the second to location 1, etc.
    template <Layout... T>
    static auto create(AttribBuilder<T...> const& builder, Primitive primitive) -> std::pair<VertexArray, Buffer> {
        constexpr auto interleaved = interleave_v<T...>;

        if (builder.vertices().empty()) {
            return {VertexArray(), Buffer()};
        }

        // Create a vertex array object.
        auto handle = GLuint{0};
        glGenVertexArrays(1, &handle);
        glBindVertexArray(handle);

        // Create a buffer object and upload vertex data.
        auto vertices = builder.vertices();
        auto bytes = std::as_bytes(std::span(vertices));
        auto buffer = Buffer::create(bytes.size(), Buffer::Usage::WRITE, Buffer::Update::INFREQUENT);
        buffer.write(bytes, 0);
        buffer.bind(Buffer::Target::VERTEX_ATTRIB);

        // Specify vertex layout.
        for (auto i = 0uz; i < interleaved.count; i++) {
            auto [base_type, elem_count, conversion] = interleaved.args[i];
            auto stride = interleaved.stride;
            auto offset = interleaved.offsets[i];
            _vertex_attrib_pointer(i, elem_count, base_type, conversion, stride, offset);
            glEnableVertexAttribArray(static_cast<GLuint>(i));
        }
        return {
            VertexArray(handle, primitive, vertices.size(), 0, Index::NONE, 0),
            std::move(buffer),
        };
    }

    template <Layout... T>
    static auto create(AttribIndexBuilder<T...> const& builder, Primitive primitive) -> std::pair<VertexArray, Buffer> {
        constexpr auto interleaved = interleave_v<T...>;

        if (builder.indices().empty()) {
            return {VertexArray(), Buffer()};
        }

        // Create a vertex array object.
        auto handle = GLuint{0};
        glGenVertexArrays(1, &handle);
        glBindVertexArray(handle);

        // Create a buffer object, upload vertex and index data.
        auto vertices = builder.vertices();
        auto vertices_bytes = std::as_bytes(std::span(vertices));
        auto indices = builder.indices();
        auto index_range = vertices.size();
        auto index_offset = vertices_bytes.size();
        auto [index_type, indices_bytes, _] = _convert_indices(index_range, indices);

        auto buffer = Buffer::create(
            vertices_bytes.size() + indices_bytes.size(),
            Buffer::Usage::WRITE,
            Buffer::Update::INFREQUENT
        );
        buffer.write(vertices_bytes, 0);
        buffer.write(indices_bytes, index_offset);
        buffer.bind(Buffer::Target::VERTEX_ATTRIB);
        buffer.bind(Buffer::Target::ELEMENT_INDEX);

        // Specify vertex layout.
        for (auto i = 0uz; i < interleaved.count; i++) {
            auto [base_type, elem_count, conversion] = interleaved.args[i];
            auto stride = interleaved.stride;
            auto offset = interleaved.offsets[i];
            _vertex_attrib_pointer(i, elem_count, base_type, conversion, stride, offset);
            glEnableVertexAttribArray(static_cast<GLuint>(i));
        }
        return {
            VertexArray(handle, primitive, indices.size(), index_range, index_type, index_offset),
            std::move(buffer),
        };
    }

private:
    Resource<GLuint, 0, decltype(&_vertex_array_deleter), &_vertex_array_deleter> _handle;
    Primitive _primitive = Primitive::POINTS;
    size_t _count = 0;
    size_t _index_range = 0;
    Index _index_type = Index::NONE;
    size_t _index_offset = 0;

    static constexpr auto _primitive_to_gl_enum(Primitive primitive) -> GLenum {
        return static_cast<GLenum>(primitive);
    }

    static constexpr auto _index_type_to_gl_enum(Index index_type) -> GLenum {
        return static_cast<GLenum>(index_type);
    }

    static void _vertex_attrib_pointer(
        size_t index,
        GLint elem_count,
        GLenum base_type,
        Conversion conversion,
        size_t stride,
        size_t offset
    ) {
        auto _index = static_cast<GLuint>(index);
        auto _stride = static_cast<GLsizei>(stride);
        auto _offset = reinterpret_cast<void const*>(offset);
        switch (conversion) {
            case Conversion::INT:
                return glVertexAttribIPointer(_index, elem_count, base_type, _stride, _offset);
            case Conversion::FLOAT:
                return glVertexAttribPointer(_index, elem_count, base_type, GL_FALSE, _stride, _offset);
            case Conversion::FLOAT_NORMALIZE:
                return glVertexAttribPointer(_index, elem_count, base_type, GL_TRUE, _stride, _offset);
            case Conversion::DOUBLE:
                return glVertexAttribLPointer(_index, elem_count, base_type, _stride, _offset);
            default:
                unreachable();
        }
    }

    // We use the fixed primitive restart index for the given index type.
    // See: https://www.khronos.org/opengl/wiki/Vertex_Rendering#Primitive_Restart
    static constexpr auto _fixed_restart_index(Index index_type) -> GLuint {
        switch (index_type) {
            case Index::UNSIGNED_BYTE:
                return std::numeric_limits<uint8_t>::max();
            case Index::UNSIGNED_SHORT:
                return std::numeric_limits<uint16_t>::max();
            case Index::UNSIGNED_INT:
                return std::numeric_limits<uint32_t>::max();
            default:
                unreachable();
        }
    }

    // The temporary storage returned by `_convert_indices`.
    using Temp = std::variant<std::vector<uint8_t>, std::vector<uint16_t>, std::monostate>;

    // Automatically selects the smallest possible index type that can hold the number of vertices
    // and returns the converted index array. Is done once per upload.
    static auto _convert_indices(size_t max_index, std::span<uint32_t const> indices)
        -> std::tuple<Index, std::span<std::byte const>, Temp> {
        // Strict less-than is needed since the maximum value is already used for primitive restart.
        if (max_index < std::numeric_limits<uint8_t>::max()) {
            auto temp = std::vector<uint8_t>(indices.size());
            for (auto i = 0uz; i < indices.size(); i++) {
                temp[i] = static_cast<uint8_t>(indices[i]);
            }
            return {Index::UNSIGNED_BYTE, std::as_bytes(std::span(temp)), std::move(temp)};
        }
        if (max_index < std::numeric_limits<uint16_t>::max()) {
            auto temp = std::vector<uint16_t>(indices.size());
            for (auto i = 0uz; i < indices.size(); i++) {
                temp[i] = static_cast<uint16_t>(indices[i]);
            }
            return {Index::UNSIGNED_SHORT, std::as_bytes(std::span(temp)), std::move(temp)};
        }
        return {Index::UNSIGNED_INT, std::as_bytes(indices), std::monostate()};
    }
};

}

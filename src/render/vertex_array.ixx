module;

#include <glad/gl.h>

export module render:vertex_array;
import std;
import types;
import debug;
import :buffer;
import :vertex_layout;

namespace render {

// Manages a GL vertex array object, similar to `std::unique_ptr`.
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
        // Compatibility options.
        // TODO: add indices support and remove this.
        QUADS = GL_QUADS,
    };

    // Constructs a `VertexArray` which owns nothing.
    VertexArray() noexcept = default;

    // Constructs a `VertexArray` which owns the given `handle`.
    // The `handle` must be either 0 or a valid GL vertex array object.
    // The `count` must not be more than the number of vertices in the object's storage.
    VertexArray(GLuint handle, Primitive primitive, size_t count) noexcept:
        _handle(handle),
        _primitive(_primitive_to_gl_enum(primitive)),
        _count(static_cast<GLsizei>(count)) {}

    VertexArray(VertexArray const&) = delete;
    VertexArray(VertexArray&& from) noexcept {
        swap(*this, from);
    }
    auto operator=(VertexArray const&) -> VertexArray& = delete;
    auto operator=(VertexArray&& from) noexcept -> VertexArray& {
        swap(*this, from);
        return *this;
    }

    ~VertexArray() {
        if (_handle != 0)
            glDeleteVertexArrays(1, &_handle);
    }

    auto get() const noexcept -> GLuint {
        return _handle;
    }

    void reset(GLuint handle) noexcept {
        if (_handle != 0)
            glDeleteVertexArrays(1, &_handle);
        _handle = handle;
    }

    explicit operator bool() const noexcept {
        return _handle != 0;
    }

    auto count() const noexcept -> size_t {
        return _count;
    }

    void bind() const {
        assert(_handle != 0, "binding an uninitialised vertex array");
        glBindVertexArray(_handle);
    }

    void render() const {
        assert(_handle != 0, "rendering an uninitialised vertex array");
        glBindVertexArray(_handle);
        glDrawArrays(_primitive, 0, _count);
    }

    // Constructs a vertex array object from a vertex array builder.
    // Returns the constructed vertex array, as well as its underlying buffer object.
    //
    // The binding locations of vertex attributes are determined by the order in which they occur
    // in the template arguments. The first attribute is bound to location 0, the second to location 1, etc.
    template <typename... T>
    static auto create(VertexArrayBuilder<T...> const& builder, Primitive primitive) -> std::pair<VertexArray, Buffer> {
        auto handle = GLuint{0};
        glGenVertexArrays(1, &handle);
        glBindVertexArray(handle);

        auto vertices = builder.vertices();
        auto bytes = std::as_bytes(std::span(vertices));
        auto buffer = Buffer::create(bytes.size(), Buffer::Usage::WRITE, Buffer::Update::INFREQUENT);
        buffer.write(bytes, 0);
        buffer.bind(Buffer::Target::VERTEX_ATTRIB);

        for (auto i = 0; i < sizeof...(T); i++) {
            auto index = static_cast<GLuint>(i);
            auto elem_count = Vertex<T...>::ATTRIB_ELEM_COUNTS[i];
            auto base_type = Vertex<T...>::ATTRIB_BASE_TYPES[i];
            auto mode = Vertex<T...>::ATTRIB_MODES[i];
            auto stride = static_cast<GLsizei>(Vertex<T...>::VERTEX_SIZE);
            auto offset = reinterpret_cast<void const*>(Vertex<T...>::ATTRIB_OFFSETS[i]);

            glEnableVertexAttribArray(index);
            switch (mode) {
                case VertexAttribMode::INTEGER:
                    glVertexAttribIPointer(index, elem_count, base_type, stride, offset);
                    break;
                case VertexAttribMode::FLOAT:
                    glVertexAttribPointer(index, elem_count, base_type, GL_FALSE, stride, offset);
                    break;
                case VertexAttribMode::FLOAT_NORMALIZE:
                    glVertexAttribPointer(index, elem_count, base_type, GL_TRUE, stride, offset);
                    break;
                case VertexAttribMode::DOUBLE:
                    glVertexAttribLPointer(index, elem_count, base_type, stride, offset);
                    break;
                default:
                    unreachable();
            }
        }
        return {VertexArray(handle, primitive, vertices.size()), std::move(buffer)};
    }

    friend void swap(VertexArray& first, VertexArray& second) noexcept {
        using std::swap;
        swap(first._handle, second._handle);
        swap(first._primitive, second._primitive);
        swap(first._count, second._count);
    }

private:
    GLuint _handle = 0;
    GLenum _primitive = 0;
    GLsizei _count = 0;

    static constexpr auto _primitive_to_gl_enum(Primitive primitive) -> GLenum {
        return static_cast<GLenum>(primitive);
    }
};

}

module;

#include <glad/gl.h>

export module render:vertex_array;
import std;
import types;
import debug;
import :buffer;

namespace render {

// Manages a GL vertex array object, similar to `std::unique_ptr`.
export class VertexArray {
public:
    // TODO: enum primitives
    // TODO: construct from builder

    VertexArray() noexcept = default;

    explicit VertexArray(GLuint handle, GLenum primitive, GLsizei count) noexcept:
        _handle(handle),
        _primitive(primitive),
        _count(count) {}

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

    friend void swap(VertexArray& first, VertexArray& second) noexcept {
        using std::swap;
        swap(first._handle, second._handle);
        swap(first._primitive, second._primitive);
        swap(first._count, second._count);
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

private:
    GLuint _handle = 0;
    GLenum _primitive = 0;
    GLsizei _count = 0;
};

}

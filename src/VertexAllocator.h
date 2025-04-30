#pragma once
#include <utility>
#include <vector>
#include <glad/gl.h>

namespace Renderer {
// TODO: implement this in VXRT first?
class VertexPool {
public:
    VertexPool() {}
    VertexPool(GLuint capacity);
    VertexPool(VertexPool const&) = delete;
    VertexPool(VertexPool&& from) noexcept:
        VertexPool() {
        swap(*this, from);
    }
    VertexPool& operator=(VertexPool const&) = delete;
    VertexPool& operator=(VertexPool&& from) noexcept {
        swap(*this, from);
        return *this;
    }

    ~VertexPool() {
        if (array_buffer != 0)
            glDeleteBuffers(1, &array_buffer);
        if (element_buffer != 0)
            glDeleteBuffers(1, &element_buffer);
    }

    friend void swap(VertexPool& first, VertexPool& second) {
        using std::swap;
        swap(first.array_buffer, second.array_buffer);
        swap(first.element_buffer, second.element_buffer);
        swap(first.capacity, second.capacity);
        swap(first.free_head, second.free_head);
        swap(first.free_list, second.free_list);
        swap(first.pending_writes, second.pending_writes);
    }

private:
    static constexpr GLuint FREE_END = static_cast<GLuint>(-1);

    GLuint array_buffer = 0;
    GLuint element_buffer = 0;
    GLuint capacity = 0;
    GLuint free_head = FREE_END;
    std::vector<GLuint> free_list;
    std::vector<std::tuple<GLuint, float>> pending_writes;
};
}

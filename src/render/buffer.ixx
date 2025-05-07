module;

#include <glad/gl.h>

export module render:buffer;
import std;
import types;
import debug;

namespace render {

// Manages a GL buffer object, similar to `std::unique_ptr`.
export class Buffer {
public:
    // Possible buffer binding points per GL 4.3.
    enum class Target : GLenum {
        VERTEX_ATTRIB = GL_ARRAY_BUFFER,
        ATOMIC_COUNTER = GL_ATOMIC_COUNTER_BUFFER,
        COPY_SRC = GL_COPY_READ_BUFFER,
        COPY_DST = GL_COPY_WRITE_BUFFER,
        COMPUTE_COMMAND = GL_DISPATCH_INDIRECT_BUFFER,
        DRAW_COMMAND = GL_DRAW_INDIRECT_BUFFER,
        INDEX = GL_ELEMENT_ARRAY_BUFFER,
        PIXEL_DST = GL_PIXEL_PACK_BUFFER,
        TEXEL_SRC = GL_PIXEL_UNPACK_BUFFER,
        STORAGE = GL_SHADER_STORAGE_BUFFER,
        TEXTURE = GL_TEXTURE_BUFFER,
        TRANSFORM_FEEDBACK = GL_TRANSFORM_FEEDBACK_BUFFER,
        UNIFORM = GL_UNIFORM_BUFFER,
    };

    // Possible mutable buffer usage hints per GL 4.3.
    enum class Usage { READ, WRITE, COPY };
    enum class Update { INFREQUENT, SEMI_FREQUENT, FREQUENT };

    // Constructs a `Buffer` which owns nothing.
    Buffer() noexcept = default;

    // Constructs a `Buffer` which owns the given `handle`.
    // The `handle` must be either 0 or a valid GL buffer object.
    explicit Buffer(GLuint handle) noexcept:
        _handle(handle) {}

    Buffer(Buffer const&) = delete;
    Buffer(Buffer&& from) noexcept {
        swap(*this, from);
    }
    auto operator=(Buffer const&) -> Buffer& = delete;
    auto operator=(Buffer&& from) noexcept -> Buffer& {
        swap(*this, from);
        return *this;
    }

    // Destroys the managed object if it owns one.
    ~Buffer() {
        if (_handle != 0)
            glDeleteBuffers(1, &_handle);
    }

    // Returns the underlying handle to the managed object.
    // The handle is 0 if it currently owns nothing.
    auto get() const noexcept -> GLuint {
        return _handle;
    }

    // Replaces the managed object.
    void reset(GLuint handle) noexcept {
        if (_handle != 0)
            glDeleteBuffers(1, &_handle);
        _handle = handle;
    }

    // Returns whether it owns a managed object.
    explicit operator bool() const noexcept {
        return _handle != 0;
    }

    // Binds the owned buffer to the given GL target.
    // Should be invoked last before a GL call to avoid accidental re-binding by other functions.
    void bind(Target target) const {
        assert(_handle != 0, "binding an unallocated buffer");
        glBindBuffer(_target_to_gl_enum(target), _handle);
    }

    // Creates a new buffer object with the given size and usage hints.
    // Invalidates any existing binding to COPY_DST target.
    static auto create(size_t size, Usage usage, Update freq) -> Buffer {
        auto handle = GLuint{0};
        auto target = _target_to_gl_enum(Target::COPY_DST);
        glGenBuffers(1, &handle);
        glBindBuffer(target, handle);
        glBufferData(target, static_cast<GLsizeiptr>(size), nullptr, _hints_to_gl_enum(usage, freq));
        return Buffer(handle);
    }

    // Reads data from the buffer.
    // The `offset + size` must not exceed the allocated size of the buffer.
    // Invalidates any existing binding to COPY_SRC target.
    auto read(size_t offset, size_t size) const -> std::vector<std::byte> {
        assert(_handle != 0, "reading from unallocated buffer");
        auto data = std::vector<std::byte>(size);
        auto target = _target_to_gl_enum(Target::COPY_SRC);
        glBindBuffer(target, _handle);
        glGetBufferSubData(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data.data());
        return data;
    }

    // Uploads data to a subset of the buffer store.
    // The `offset + data.size()` must not exceed the allocated size of the buffer.
    // Invalidates any existing binding to COPY_DST target.
    void write(std::span<std::byte const> data, size_t offset) const {
        assert(_handle != 0, "uploading to unallocated buffer");
        auto target = _target_to_gl_enum(Target::COPY_DST);
        glBindBuffer(target, _handle);
        glBufferSubData(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(data.size()), data.data());
    }

    // Copies data from another buffer.
    // The `offset + size` must not exceed the allocated size of either buffer.
    // Invalidates any existing binding to COPY_SRC and COPY_DST targets.
    void copy(Buffer const& src, size_t src_offset, size_t src_size, size_t offset) const {
        assert(src._handle != 0, "copying from unallocated buffer");
        assert(_handle != 0, "copying to unallocated buffer");
        auto src_target = _target_to_gl_enum(Target::COPY_SRC);
        auto dst_target = _target_to_gl_enum(Target::COPY_DST);
        glBindBuffer(src_target, src._handle);
        glBindBuffer(dst_target, _handle);
        glCopyBufferSubData(
            src_target,
            dst_target,
            static_cast<GLintptr>(src_offset),
            static_cast<GLintptr>(offset),
            static_cast<GLsizeiptr>(src_size)
        );
    }

    // Swaps the managed objects of two `Buffer`s.
    friend void swap(Buffer& first, Buffer& second) noexcept {
        using std::swap;
        swap(first._handle, second._handle);
    }

private:
    GLuint _handle = 0;

    static constexpr auto _target_to_gl_enum(Target target) -> GLenum {
        return static_cast<GLenum>(target);
    }

    static constexpr auto _hints_to_gl_enum(Usage usage, Update freq) -> GLenum {
        switch (usage) {
            case Usage::READ:
                return freq == Update::INFREQUENT    ? GL_STATIC_READ
                     : freq == Update::SEMI_FREQUENT ? GL_DYNAMIC_READ
                                                     : GL_STREAM_READ;
            case Usage::WRITE:
                return freq == Update::INFREQUENT    ? GL_STATIC_DRAW
                     : freq == Update::SEMI_FREQUENT ? GL_DYNAMIC_DRAW
                                                     : GL_STREAM_DRAW;
            case Usage::COPY:
                return freq == Update::INFREQUENT    ? GL_STATIC_COPY
                     : freq == Update::SEMI_FREQUENT ? GL_DYNAMIC_COPY
                                                     : GL_STREAM_COPY;
            default:
                unreachable();
        }
    }
};

}

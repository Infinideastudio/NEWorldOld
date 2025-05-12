module;

#include <glad/gl.h>

export module render:buffer;
import std;
import types;
import debug;
import :types;

namespace render {

// Helper function to delete a valid GL buffer object.
void _buffer_deleter(GLuint handle) {
    glDeleteBuffers(1, &handle);
}

// GL only provides API to unmap the whole buffer, which is expected since each buffer may
// have at most one active mapping at a time.
void _buffer_unmapper(GLuint handle) {
    glBindBuffer(GL_COPY_READ_BUFFER, handle);
    glUnmapBuffer(GL_COPY_READ_BUFFER);
}

// Manages a GL buffer object.
export class Buffer {
public:
    // Possible non-indexed buffer binding points per GL 4.3.
    enum class Target : GLenum {
        VERTEX_ATTRIB = GL_ARRAY_BUFFER,
        COPY_SRC = GL_COPY_READ_BUFFER,
        COPY_DST = GL_COPY_WRITE_BUFFER,
        COMPUTE_COMMAND = GL_DISPATCH_INDIRECT_BUFFER,
        DRAW_COMMAND = GL_DRAW_INDIRECT_BUFFER,
        ELEMENT_INDEX = GL_ELEMENT_ARRAY_BUFFER,
        PIXEL_DST = GL_PIXEL_PACK_BUFFER,
        TEXEL_SRC = GL_PIXEL_UNPACK_BUFFER,
        STORAGE_TEXTURE = GL_TEXTURE_BUFFER,
    };

    // Possible indexed buffer binding points per GL 4.3.
    enum class IndexedTarget : GLenum {
        ATOMIC_COUNTER = GL_ATOMIC_COUNTER_BUFFER,
        STORAGE = GL_SHADER_STORAGE_BUFFER,
        TRANSFORM_FEEDBACK = GL_TRANSFORM_FEEDBACK_BUFFER,
        UNIFORM = GL_UNIFORM_BUFFER,
    };

    // Possible mutable buffer usage hints per GL 4.3.
    enum class Usage { READ, WRITE, COPY };
    enum class Update { INFREQUENT, SEMI_FREQUENT, FREQUENT };

    // Manages a mapped region of a GL buffer object.
    template <typename T>
    class MappedRegion;

    // Constructs a `Buffer` which owns nothing.
    Buffer() noexcept = default;

    // Constructs a `Buffer` which owns the given `handle`.
    // The `handle` must be either 0 or a valid GL buffer object.
    // The `size` must be the size of the buffer storage in bytes.
    explicit Buffer(GLuint handle, size_t size) noexcept:
        _handle(handle),
        _size(size) {}

    // Returns the underlying handle to the managed object.
    // The handle is 0 if it currently owns nothing.
    auto get() const noexcept -> GLuint {
        return _handle.get();
    }

    // Returns the size of the managed object in bytes.
    // The size is 0 if it currently owns nothing.
    auto size() const noexcept -> size_t {
        return _size;
    }

    // Returns whether it owns a managed object.
    operator bool() const noexcept {
        return bool(_handle);
    }

    // Creates a new buffer object with the given size and usage hints.
    // Invalidates any existing binding to `COPY_DST` target.
    static auto create(size_t size, Usage usage, Update freq) -> Buffer {
        auto handle = GLuint{0};
        auto target = _target_to_gl_enum(Target::COPY_DST);
        glGenBuffers(1, &handle);
        glBindBuffer(target, handle);
        glBufferData(target, static_cast<GLsizeiptr>(size), nullptr, _hints_to_gl_enum(usage, freq));
        return Buffer(handle, size);
    }

    // Binds the owned buffer to the given GL target (non-indexed).
    // Should be invoked last before a GL call to avoid accidental re-binding by other functions.
    void bind(Target target) const {
        assert(*this, "binding an unallocated buffer");
        glBindBuffer(_target_to_gl_enum(target), get());
    }

    // Binds the owned buffer to the given GL target (indexed).
    // Should be invoked last before a GL call to avoid accidental re-binding by other functions.
    void bind(IndexedTarget target, size_t index) const {
        assert(*this, "binding an unallocated buffer");
        glBindBufferBase(_target_to_gl_enum(target), static_cast<GLuint>(index), get());
    }

    // Reads data from the buffer.
    // Invalidates any existing binding to `COPY_SRC` target.
    auto read(size_t offset, size_t length) const -> std::vector<std::byte> {
        assert(*this, "reading from unallocated buffer");
        assert(offset + length <= size(), "reading out of bounds");
        auto data = std::vector<std::byte>(length);
        auto target = _target_to_gl_enum(Target::COPY_SRC);
        glBindBuffer(target, get());
        glGetBufferSubData(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(length), data.data());
        return data;
    }

    // Uploads data to a subset of the buffer store.
    // Invalidates any existing binding to `COPY_DST` target.
    void write(std::span<std::byte const> data, size_t offset) const {
        assert(*this, "uploading to unallocated buffer");
        assert(offset + data.size() <= size(), "uploading out of bounds");
        auto target = _target_to_gl_enum(Target::COPY_DST);
        glBindBuffer(target, get());
        glBufferSubData(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(data.size()), data.data());
    }

    // Copies data from another buffer.
    // Invalidates any existing binding to COPY_SRC and `COPY_DST` targets.
    void copy(Buffer const& src, size_t src_offset, size_t length, size_t offset) const {
        assert(src, "copying from unallocated buffer");
        assert(*this, "copying to unallocated buffer");
        assert(src_offset + length <= src.size(), "copying out of bounds");
        assert(offset + length <= size(), "copying out of bounds");
        auto src_target = _target_to_gl_enum(Target::COPY_SRC);
        auto dst_target = _target_to_gl_enum(Target::COPY_DST);
        glBindBuffer(src_target, src.get());
        glBindBuffer(dst_target, get());
        glCopyBufferSubData(
            src_target,
            dst_target,
            static_cast<GLintptr>(src_offset),
            static_cast<GLintptr>(offset),
            static_cast<GLsizeiptr>(length)
        );
    }

    // Maps the buffer to a memory region for read-only access.
    // The returned region is valid until `this` is destroyed. Only one mapping is allowed on a
    // single buffer at a time.
    // Invalidates any existing binding to `COPY_SRC` target until the region is destroyed.
    auto map_read(size_t offset, size_t size, bool synchronize = true) const -> MappedRegion<std::byte const>;

    // Maps the buffer to a memory region for read-write access.
    // The returned region is valid until `this` is destroyed. Only one mapping is allowed on a
    // single buffer at a time.
    // Invalidates any existing binding to `COPY_SRC` target until the region is destroyed.
    auto map_write(
        size_t offset,
        size_t size,
        bool synchronize = true,
        bool write_only = false,
        bool invalidate_range = false,
        bool auto_flush = true
    ) const -> MappedRegion<std::byte>;

private:
    Resource<GLuint, 0, decltype(&_buffer_deleter), &_buffer_deleter> _handle;
    size_t _size = 0;

    static constexpr auto _target_to_gl_enum(Target target) -> GLenum {
        return static_cast<GLenum>(target);
    }

    static constexpr auto _target_to_gl_enum(IndexedTarget target) -> GLenum {
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

template <typename T>
class Buffer::MappedRegion {
public:
    MappedRegion() noexcept = default;

    explicit MappedRegion(GLuint parent_handle, size_t parent_offset, T* mapped_address, size_t mapped_size) noexcept:
        _parent_handle(parent_handle),
        _parent_offset(parent_offset),
        _address_space(mapped_address, mapped_size) {}

    operator bool() const noexcept {
        return bool(_parent_handle);
    }

    auto parent() const noexcept -> GLuint {
        return _parent_handle.get();
    }

    auto parent_offset() const noexcept -> size_t {
        return _parent_offset;
    }

    auto bytes() const noexcept -> std::span<T> {
        return _address_space;
    }

    // Explicitly flushes changes to a sub-region of the mapped buffer.
    // The mapping must have `auto_flush` set to false.
    void flush(size_t offset, size_t length) const {
        assert(*this, "flushing undefined region");
        assert(offset + length <= bytes().size(), "flushing out of bounds");
        auto target = _target_to_gl_enum(Target::COPY_SRC);
        glBindBuffer(target, parent());
        glFlushMappedBufferRange(
            target,
            static_cast<GLintptr>(parent_offset() + offset),
            static_cast<GLsizeiptr>(length)
        );
    }

private:
    Resource<GLuint, 0, decltype(&_buffer_unmapper), &_buffer_unmapper> _parent_handle;
    size_t _parent_offset = 0;
    std::span<T> _address_space = {};
};

auto Buffer::map_read(size_t offset, size_t length, bool synchronize) const -> MappedRegion<std::byte const> {
    assert(*this, "mapping unallocated buffer");
    assert(offset + length <= size(), "mapping out of bounds");
    auto target = _target_to_gl_enum(Target::COPY_SRC);
    auto access = GL_MAP_READ_BIT | (synchronize ? 0 : GL_MAP_UNSYNCHRONIZED_BIT);
    glBindBuffer(target, get());
    auto data = glMapBufferRange(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(length), access);
    return MappedRegion(get(), offset, static_cast<std::byte const*>(data), length);
}

auto Buffer::map_write(
    size_t offset,
    size_t length,
    bool synchronize,
    bool write_only,
    bool invalidate_range,
    bool auto_flush
) const -> MappedRegion<std::byte> {
    assert(*this, "mapping unallocated buffer");
    assert(offset + length <= size(), "mapping out of bounds");
    auto target = _target_to_gl_enum(Target::COPY_SRC);
    auto access = GL_MAP_WRITE_BIT | (synchronize ? 0 : GL_MAP_UNSYNCHRONIZED_BIT) | (write_only ? 0 : GL_MAP_READ_BIT)
                | (invalidate_range ? GL_MAP_INVALIDATE_RANGE_BIT : 0) | (auto_flush ? 0 : GL_MAP_FLUSH_EXPLICIT_BIT);
    glBindBuffer(target, get());
    auto data = glMapBufferRange(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(length), access);
    return MappedRegion(get(), offset, static_cast<std::byte*>(data), length);
}

}

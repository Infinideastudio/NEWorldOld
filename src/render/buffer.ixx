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
        TEXTURE = GL_TEXTURE_BUFFER,
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
    Buffer(GLuint handle, size_t size) noexcept:
        _handle(handle),
        _size(size) {}

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
        if (_handle != 0) {
            glDeleteBuffers(1, &_handle);
        }
    }

    // Returns the underlying handle to the managed object.
    // The handle is 0 if it currently owns nothing.
    auto get() const noexcept -> GLuint {
        return _handle;
    }

    // Returns whether it owns a managed object.
    explicit operator bool() const noexcept {
        return _handle != 0;
    }

    // Binds the owned buffer to the given GL target (non-indexed).
    // Should be invoked last before a GL call to avoid accidental re-binding by other functions.
    void bind(Target target) const {
        assert(_handle != 0, "binding an unallocated buffer");
        glBindBuffer(_target_to_gl_enum(target), _handle);
    }

    // Binds the owned buffer to the given GL target (indexed).
    // Should be invoked last before a GL call to avoid accidental re-binding by other functions.
    void bind(IndexedTarget target, size_t index) const {
        assert(_handle != 0, "binding an unallocated buffer");
        glBindBufferBase(_target_to_gl_enum(target), static_cast<GLuint>(index), _handle);
    }

    // Creates a new buffer object with the given size and usage hints.
    // Invalidates any existing binding to `COPY_DST` target.
    static auto create(size_t size, Usage usage, Update freq) -> Buffer {
        auto handle = GLuint{0};
        auto target = _target_to_gl_enum(Target::COPY_DST);
        glGenBuffers(1, &handle);
        glBindBuffer(target, handle);
        glBufferData(target, static_cast<GLsizeiptr>(size), nullptr, _hints_to_gl_enum(usage, freq));
        return {handle, size};
    }

    // Reads data from the buffer.
    // The `offset + size` must not exceed the allocated size of the buffer.
    // Invalidates any existing binding to `COPY_SRC` target.
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
    // Invalidates any existing binding to `COPY_DST` target.
    void write(std::span<std::byte const> data, size_t offset) const {
        assert(_handle != 0, "uploading to unallocated buffer");
        auto target = _target_to_gl_enum(Target::COPY_DST);
        glBindBuffer(target, _handle);
        glBufferSubData(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(data.size()), data.data());
    }

    // Copies data from another buffer.
    // The `offset + size` must not exceed the allocated size of either buffer.
    // Invalidates any existing binding to COPY_SRC and `COPY_DST` targets.
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

    // Swaps the managed objects of two `Buffer`s.
    friend void swap(Buffer& first, Buffer& second) noexcept {
        using std::swap;
        swap(first._handle, second._handle);
        swap(first._size, second._size);
    }

private:
    GLuint _handle = 0;
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

    MappedRegion(GLuint parent_handle, size_t parent_offset, T* mapped_address, size_t mapped_size) noexcept:
        _parent_handle(parent_handle),
        _parent_offset(parent_offset),
        _address_space(mapped_address, mapped_size) {}

    MappedRegion(MappedRegion const&) = delete;
    MappedRegion(MappedRegion&& from) noexcept {
        swap(*this, from);
    }
    auto operator=(MappedRegion const&) -> MappedRegion& = delete;
    auto operator=(MappedRegion&& from) noexcept -> MappedRegion& {
        swap(*this, from);
        return *this;
    }

    ~MappedRegion() {
        // GL only provides API to unmap the whole buffer, which is expected since each buffer may
        // have at most one active mapping at a time.
        if (_parent_handle != 0) {
            auto target = _target_to_gl_enum(Target::COPY_SRC);
            glBindBuffer(target, _parent_handle);
            glUnmapBuffer(target);
        }
    }

    explicit operator bool() const noexcept {
        return _parent_handle != 0;
    }

    auto region() const noexcept -> std::span<T> {
        return _address_space;
    }

    auto get() const noexcept -> T* {
        return _address_space.data();
    }

    auto size() const noexcept -> size_t {
        return _address_space.size();
    }

    // Explicitly flushes changes to a sub-region of the mapped buffer.
    // The mapping must have `auto_flush` set to false.
    void flush(size_t offset, size_t size) const {
        assert(_parent_handle != 0, "flushing undefined region");
        assert(offset + size <= _address_space.size(), "flushing out of bounds");
        auto target = _target_to_gl_enum(Target::COPY_SRC);
        glBindBuffer(target, _parent_handle);
        glFlushMappedBufferRange(target, static_cast<GLintptr>(_parent_offset + offset), static_cast<GLsizeiptr>(size));
    }

    friend void swap(MappedRegion& first, MappedRegion& second) noexcept {
        using std::swap;
        swap(first._parent_handle, second._parent_handle);
        swap(first._parent_offset, second._parent_offset);
        swap(first._address_space, second._address_space);
    }

private:
    GLuint _parent_handle = 0;
    size_t _parent_offset = 0;
    std::span<T> _address_space = {};
};

auto Buffer::map_read(size_t offset, size_t size, bool synchronize) const -> MappedRegion<std::byte const> {
    assert(_handle != 0, "mapping unallocated buffer");
    assert(offset + size <= _size, "mapping out of bounds");
    auto target = _target_to_gl_enum(Target::COPY_SRC);
    auto access = GL_MAP_READ_BIT | (synchronize ? 0 : GL_MAP_UNSYNCHRONIZED_BIT);
    glBindBuffer(target, _handle);
    auto data = glMapBufferRange(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), access);
    return {_handle, offset, static_cast<std::byte const*>(data), size};
}

auto Buffer::map_write(
    size_t offset,
    size_t size,
    bool synchronize,
    bool write_only,
    bool invalidate_range,
    bool auto_flush
) const -> MappedRegion<std::byte> {
    assert(_handle != 0, "mapping unallocated buffer");
    assert(offset + size <= _size, "mapping out of bounds");
    auto target = _target_to_gl_enum(Target::COPY_SRC);
    auto access = GL_MAP_WRITE_BIT | (synchronize ? 0 : GL_MAP_UNSYNCHRONIZED_BIT) | (write_only ? 0 : GL_MAP_READ_BIT)
                | (invalidate_range ? GL_MAP_INVALIDATE_RANGE_BIT : 0) | (auto_flush ? 0 : GL_MAP_FLUSH_EXPLICIT_BIT);
    glBindBuffer(target, _handle);
    auto data = glMapBufferRange(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), access);
    return {_handle, offset, static_cast<std::byte*>(data), size};
}

}

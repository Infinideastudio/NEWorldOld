module;

#include <glad/gl.h>

export module render:texture;
import std;
import types;
import debug;
import :image;

namespace render {

// Manages a GL 2D array texture object with mutable storage, similar to `std::unique_ptr`.
// Note that 2D array textures cover most of the use cases for textures, and are likely optimized.
export class Texture {
public:
    // Possible internal formats per GL 4.3.
    // Currently, only the base internal formats are considered.
    enum class Format : GLenum {
        DEPTH = GL_DEPTH_COMPONENT,
        DEPTH_STENCIL = GL_DEPTH_STENCIL,
        STENCIL = GL_STENCIL_INDEX,
        R = GL_RED,
        RG = GL_RG,
        RGB = GL_RGB,
        RGBA = GL_RGBA,
    };

    // Possible depth-stencil texture access modes per GL 4.3.
    enum class DepthStencilMode : GLenum {
        DEPTH = GL_DEPTH_COMPONENT,
        STENCIL = GL_STENCIL_INDEX,
    };

    // Possible depth texture comparison modes per GL 4.3.
    enum class DepthCompareMode : GLenum {
        NONE = GL_NONE,
        LEQUAL = GL_LEQUAL,
        GEQUAL = GL_GEQUAL,
        LESS = GL_LESS,
        GREATER = GL_GREATER,
        EQUAL = GL_EQUAL,
        NOTEQUAL = GL_NOTEQUAL,
        ALWAYS = GL_ALWAYS,
        NEVER = GL_NEVER,
    };

    // Constructs a `Texture` which owns nothing.
    Texture() noexcept = default;

    // Constructs a `Texture` which owns the given `handle`.
    // The `handle` must be either 0 or a valid GL 2D array texture object.
    // The `format` must be the internal format of the texture array.
    // The `width` and `height` must be the dimensions of the texture array in pixels.
    // The `depth` must be the number of layers in the texture array.
    explicit Texture(GLuint handle, Format format, size_t width, size_t height, size_t depth = 1) noexcept:
        _handle(handle),
        _format(format),
        _width(width),
        _height(height),
        _depth(depth) {}

    Texture(Texture const&) = delete;
    Texture(Texture&& from) noexcept {
        swap(*this, from);
    }
    auto operator=(Texture const&) -> Texture& = delete;
    auto operator=(Texture&& from) noexcept -> Texture& {
        swap(*this, from);
        return *this;
    }

    // Destroys the managed object if it owns one.
    ~Texture() {
        if (_handle != 0) {
            glDeleteTextures(1, &_handle);
        }
    }

    auto width() const noexcept -> size_t {
        return _width;
    }

    auto height() const noexcept -> size_t {
        return _height;
    }

    auto depth() const noexcept -> size_t {
        return _depth;
    }

    auto get() const noexcept -> GLuint {
        return _handle;
    }

    // Returns whether it owns a managed object.
    explicit operator bool() const noexcept {
        return _handle != 0;
    }

    // Creates a new 2D array texture object with the given dimensions and internal format.
    // Invalidates any existing binding to texture unit 0.
    static auto create(Format format, size_t width, size_t height, size_t depth = 1) -> Texture {
        auto handle = GLuint{0};
        glGenTextures(1, &handle);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, handle);
        // Set default wrapping parameters.
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        // Set default filtering parameters.
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        // Create the texture storage.
        // TODO: change to glTexStorage3D when migrating to GL 4.3
        glTexImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            static_cast<GLint>(_format_to_gl_enum(format)),
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            static_cast<GLsizei>(depth),
            0,
            _format_to_gl_enum(format),
            GL_UNSIGNED_BYTE,
            nullptr
        );
        return Texture(handle, format, width, height, depth);
    }

    // Fills a region within a single layer using pixels from an image.
    // Invalidates any existing binding to texture unit 0.
    // Currently, only color textures and Vec*u8 images are supported.
    template <typename T, size_t N>
    requires (1 <= N && N <= 4)
    void fill(size_t x, size_t y, size_t z, Image<Vector<T, N>> const& src, size_t src_z = 0) {
        assert(x + src.width() <= width() && y + src.height() <= height() && z + 1 <= depth());
        assert(src_z + 1 <= src.depth());
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, _handle);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            static_cast<GLint>(x),
            static_cast<GLint>(y),
            static_cast<GLint>(z),
            static_cast<GLsizei>(src.width()),
            static_cast<GLsizei>(src.height()),
            1,
            _elem_count_to_input_format<N>(),
            _elem_type_to_input_type<T>(),
            &src[0, 0, src_z]
        );
    }

    // Binds the owned array texture to the given texture unit.
    void bind(size_t index) const {
        assert(_handle != 0, "binding an unallocated texture");
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + index));
        glBindTexture(GL_TEXTURE_2D_ARRAY, _handle);
    }

    // Sets the wrapping parameters for the 2D array texture.
    // Invalidates any existing binding to texture unit 0.
    void set_wrap(bool repeat = false) {
        assert(_handle != 0, "setting wrap parameters on an unallocated texture");
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, _handle);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_R, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    }

    // Sets the filtering parameters for the 2D array texture.
    // Invalidates any existing binding to texture unit 0.
    void set_filter(bool bilinear = false, bool mipmap = false) {
        assert(_handle != 0, "setting filter parameters on an unallocated texture");
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, _handle);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
        if (mipmap) {
            glTexParameteri(
                GL_TEXTURE_2D_ARRAY,
                GL_TEXTURE_MIN_FILTER,
                bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR
            );
        }
    }

    // Sets the depth-stencil texture access mode.
    // Invalidates any existing binding to texture unit 0.
    void set_depth_stencil_mode(DepthStencilMode mode) {
        assert(_handle != 0, "setting depth-stencil mode on an unallocated texture");
        auto value = _depth_stencil_mode_to_gl_enum(mode);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, _handle);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_DEPTH_STENCIL_TEXTURE_MODE, static_cast<GLint>(value));
    }

    // Sets the depth texture comparison mode.
    // Invalidates any existing binding to texture unit 0.
    void set_depth_compare_mode(DepthCompareMode mode) {
        assert(_handle != 0, "setting depth compare mode on an unallocated texture");
        auto value = _depth_compare_mode_to_gl_enum(mode);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, _handle);
        if (mode == DepthCompareMode::NONE) {
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        } else {
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, static_cast<GLint>(value));
        }
    }

    // Recomputes the mipmap levels for the 2D array texture.
    // Invalidates any existing binding to texture unit 0.
    void generate_mipmaps() {
        assert(_handle != 0, "generating mipmaps for an unallocated texture");
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, _handle);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    }

    // Swaps the managed objects of two `Texture`s.
    friend void swap(Texture& first, Texture& second) noexcept {
        using std::swap;
        swap(first._handle, second._handle);
        swap(first._width, second._width);
        swap(first._height, second._height);
        swap(first._depth, second._depth);
    }

private:
    GLuint _handle = 0;
    Format _format = Format::DEPTH;
    size_t _width = 0;
    size_t _height = 0;
    size_t _depth = 0;

    static constexpr auto _format_to_gl_enum(Format format) -> GLenum {
        return static_cast<GLenum>(format);
    }

    static constexpr auto _depth_stencil_mode_to_gl_enum(DepthStencilMode mode) -> GLenum {
        return static_cast<GLenum>(mode);
    }

    static constexpr auto _depth_compare_mode_to_gl_enum(DepthCompareMode mode) -> GLenum {
        return static_cast<GLenum>(mode);
    }

    template <size_t N>
    static constexpr auto _elem_count_to_input_format() -> GLenum {
        if constexpr (N == 1) {
            return GL_RED;
        } else if constexpr (N == 2) {
            return GL_RG;
        } else if constexpr (N == 3) {
            return GL_RGB;
        } else if constexpr (N == 4) {
            return GL_RGBA;
        } else {
            unreachable();
        }
    }

    template <typename T>
    static constexpr auto _elem_type_to_input_type() -> GLenum {
        if constexpr (std::is_same_v<T, int8_t>) {
            return GL_BYTE;
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            return GL_UNSIGNED_BYTE;
        } else if constexpr (std::is_same_v<T, int16_t>) {
            return GL_SHORT;
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            return GL_UNSIGNED_SHORT;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return GL_INT;
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return GL_UNSIGNED_INT;
        } else if constexpr (std::is_same_v<T, float>) {
            return GL_FLOAT;
        } else {
            unreachable();
        }
    }
};

}

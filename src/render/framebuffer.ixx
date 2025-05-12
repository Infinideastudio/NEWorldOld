module;

#include <glad/gl.h>

export module render:framebuffer;
import std;
import types;
import debug;
import :types;
import :texture;

namespace render {

// Helper function to delete a valid GL framebuffer object.
void _framebuffer_deleter(GLuint handle) {
    glDeleteFramebuffers(1, &handle);
}

// Manages a GL framebuffer object.
export class Framebuffer {
public:
    // Possible framebuffer binding targets per GL 4.3.
    enum class Target : GLenum {
        WRITE = GL_DRAW_FRAMEBUFFER,
        READ = GL_READ_FRAMEBUFFER,
    };

    // A 2D array texture attachment to a framebuffer.
    struct Attachment {
        std::reference_wrapper<Texture> texture;
        size_t mipmap_level;
        size_t layer;
    };

    // Constructs a `Framebuffer` which owns nothing.
    Framebuffer() noexcept = default;

    // Constructs a `Framebuffer` which owns the given `handle`.
    // The `handle` must be either 0 or a valid GL framebuffer object.
    // The `width` and `height` are the dimensions of the framebuffer (minimum size among all attachments).
    explicit Framebuffer(GLuint handle, size_t width, size_t height) noexcept:
        _handle(handle),
        _width(width),
        _height(height) {}

    // Returns the underlying handle to the managed object.
    // The handle is 0 if it currently owns nothing.
    auto get() const noexcept -> GLuint {
        return _handle.get();
    }

    // Returns the width of the framebuffer.
    // This is the minimum width of all attachments.
    auto width() const noexcept -> size_t {
        return _width;
    }

    // Returns the height of the framebuffer.
    // This is the minimum height of all attachments.
    auto height() const noexcept -> size_t {
        return _height;
    }

    // Returns whether it owns a managed object.
    operator bool() const noexcept {
        return bool(_handle);
    }

    // Creates a new framebuffer object with the given texture attachments.
    // Invalidates any existing binding to `WRITE` and `READ` targets.
    static auto create(
        std::vector<Attachment> const& colors,
        std::optional<Attachment> const& depth,
        std::optional<Attachment> const& stencil
    ) -> std::expected<Framebuffer, std::string> {
        auto handle = GLuint{0};
        auto write_target = _target_to_gl_enum(Target::WRITE);
        auto read_target = _target_to_gl_enum(Target::READ);
        glGenFramebuffers(1, &handle);
        glBindFramebuffer(write_target, handle);
        glBindFramebuffer(read_target, handle);

        // Calculate the size of the framebuffer.
        auto width = std::numeric_limits<size_t>::max();
        auto height = std::numeric_limits<size_t>::max();
        for (auto const& [texture, mipmap_level, layer]: colors) {
            auto const& tex = texture.get();
            auto image_width = std::max(1uz, tex.width() / (1uz << mipmap_level));
            auto image_height = std::max(1uz, tex.height() / (1uz << mipmap_level));
            width = std::min(width, image_width);
            height = std::min(height, image_height);
        }
        if (depth) {
            auto const& [texture, mipmap_level, layer] = *depth;
            auto const& tex = texture.get();
            auto image_width = std::max(1uz, tex.width() / (1uz << mipmap_level));
            auto image_height = std::max(1uz, tex.height() / (1uz << mipmap_level));
            width = std::min(width, image_width);
            height = std::min(height, image_height);
        }
        if (stencil) {
            auto const& [texture, mipmap_level, layer] = *stencil;
            auto const& tex = texture.get();
            auto image_width = std::max(1uz, tex.width() / (1uz << mipmap_level));
            auto image_height = std::max(1uz, tex.height() / (1uz << mipmap_level));
            width = std::min(width, image_width);
            height = std::min(height, image_height);
        }

        // Attach the color attachments.
        for (auto i = 0uz; i < colors.size(); i++) {
            auto const& [texture, mipmap_level, layer] = colors[i];
            glFramebufferTextureLayer(
                write_target,
                GL_COLOR_ATTACHMENT0 + i,
                texture.get().get(),
                static_cast<GLint>(mipmap_level),
                static_cast<GLint>(layer)
            );
        }

        // Attach the depth attachment if present.
        if (depth) {
            auto const& [texture, mipmap_level, layer] = *depth;
            glFramebufferTextureLayer(
                write_target,
                GL_DEPTH_ATTACHMENT,
                texture.get().get(),
                static_cast<GLint>(mipmap_level),
                static_cast<GLint>(layer)
            );
        }

        // Attach the stencil attachment if present.
        if (stencil) {
            auto const& [texture, mipmap_level, layer] = *stencil;
            glFramebufferTextureLayer(
                write_target,
                GL_STENCIL_ATTACHMENT,
                texture.get().get(),
                static_cast<GLint>(mipmap_level),
                static_cast<GLint>(layer)
            );
        }

        // Set the attachments to draw to. This includes all of the color attachments.
        auto draw_buffers = std::vector<GLenum>(colors.size());
        for (auto i = 0uz; i < colors.size(); i++) {
            draw_buffers[i] = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
        }
        glDrawBuffers(static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data());

        // Set the attachment to read from. This is the first color attachment if present.
        if (!colors.empty()) {
            glReadBuffer(GL_COLOR_ATTACHMENT0);
        } else if (depth) {
            glReadBuffer(GL_DEPTH_ATTACHMENT);
        } else if (stencil) {
            glReadBuffer(GL_STENCIL_ATTACHMENT);
        } else {
            glReadBuffer(GL_NONE);
        }

        // Check for completeness.
        auto status = glCheckFramebufferStatus(write_target);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            glDeleteFramebuffers(1, &handle);
            switch (status) {
                case GL_FRAMEBUFFER_UNDEFINED:
                    return std::unexpected("framebuffer is undefined");
                case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
                    return std::unexpected("an attachment is incomplete");
                case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
                    return std::unexpected("framebuffer cannot be empty");
                case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
                    return std::unexpected("framebuffer must have color attachments");
                case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
                    return std::unexpected("framebuffer cannot be empty");
                case GL_FRAMEBUFFER_UNSUPPORTED:
                    return std::unexpected("combination of attachments is not supported");
                default:
                    return std::unexpected("unknown error");
            }
        }
        return Framebuffer(handle, width, height);
    }

    // Binds the owned framebuffer to the given GL target.
    // Should be invoked last before a GL call to avoid accidental re-binding by other functions.
    void bind(Target target) const {
        assert(*this, "binding an unallocated framebuffer");
        glBindFramebuffer(_target_to_gl_enum(target), get());
    }

    // Binds the default framebuffer to the given GL target.
    // Should be invoked last before a GL call to avoid accidental re-binding by other functions.
    static void bind_default(Target target) {
        glBindFramebuffer(_target_to_gl_enum(target), 0);
    }

private:
    Resource<GLuint, 0, decltype(&_framebuffer_deleter), &_framebuffer_deleter> _handle;
    size_t _width = 0;
    size_t _height = 0;

    static constexpr auto _target_to_gl_enum(Target target) -> GLenum {
        return static_cast<GLenum>(target);
    }
};

}

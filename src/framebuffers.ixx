module;

#include <cassert>
#include <glad/gl.h>

export module framebuffers;
import std;
import types;
import globals;

export class Framebuffer {
public:
    Framebuffer() = default;
    Framebuffer(int width, int height, int cnt, bool depth, bool shadow = false, bool bilinear = false);
    Framebuffer(Framebuffer const&) = delete;
    Framebuffer(Framebuffer&& from) noexcept:
        Framebuffer() {
        swap(*this, from);
    }
    auto operator=(Framebuffer const&) -> Framebuffer& = delete;
    auto operator=(Framebuffer&& from) noexcept -> Framebuffer& {
        swap(*this, from);
        return *this;
    }

    friend void swap(Framebuffer& first, Framebuffer& second) noexcept {
        using std::swap;
        swap(first.w, second.w);
        swap(first.h, second.h);
        swap(first.id, second.id);
        swap(first.depthTexture, second.depthTexture);
        swap(first.depthRenderbuffer, second.depthRenderbuffer);
        swap(first.colorTextures, second.colorTextures);
    }

    ~Framebuffer() {
        if (id != 0)
            glDeleteFramebuffers(1, &id);
        if (depthTexture != 0)
            glDeleteTextures(1, &depthTexture);
        if (depthRenderbuffer != 0)
            glDeleteRenderbuffers(1, &depthRenderbuffer);
        glDeleteTextures(static_cast<GLsizei>(colorTextures.size()), colorTextures.data());
    }

    auto empty() const -> bool {
        return id == 0;
    }
    auto width() const -> int {
        return w;
    }
    auto height() const -> int {
        return h;
    }

    void bindTarget(std::vector<GLuint> indices);
    void bindTargets();
    void unbindTarget() const;
    void bindDepthTexture(GLuint number = 0) const;
    void bindColorTexture(GLuint index, GLuint number = 0) const;
    void bindColorTextures(GLuint startNumber = 0) const;
    void copyDepthTexture(Framebuffer& target) const;

private:
    int w = 0;
    int h = 0;
    GLuint id = 0;
    GLuint depthTexture = 0;
    GLuint depthRenderbuffer = 0;
    std::vector<GLuint> colorTextures;
};

Framebuffer::Framebuffer(int width, int height, int cnt, bool depth, bool shadow, bool bilinear):
    w(width),
    h(height) {
    // Create framebuffer object
    glGenFramebuffers(1, &id);
    glBindFramebuffer(GL_FRAMEBUFFER, id);

    if (depth) {
        // Create depth texture
        glGenTextures(1, &depthTexture);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, shadow ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shadow ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        if (shadow) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        // Attach
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
    } else {
        // Create depth renderbuffer
        glGenRenderbuffers(1, &depthRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        // Attach
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);
    }

    // Create color textures
    colorTextures.resize(cnt);
    glGenTextures(cnt, colorTextures.data());
    for (int i = 0; i < cnt; i++) {
        glBindTexture(GL_TEXTURE_2D, colorTextures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        // Attach
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorTextures[i], 0);
    }

    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::bindTarget(std::vector<GLuint> indices) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id);
    std::vector<GLuint> arr;
    for (GLuint i: indices) {
        assert(i < colorTextures.size());
        arr.emplace_back(GL_COLOR_ATTACHMENT0 + i);
    }
    glDrawBuffers(static_cast<GLsizei>(arr.size()), arr.data());
    glViewport(0, 0, w, h);
}

void Framebuffer::bindTargets() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id);
    if (colorTextures.empty()) {
        glDrawBuffer(GL_NONE);
    } else {
        std::vector<GLuint> arr;
        for (int i = 0; i < colorTextures.size(); i++)
            arr.emplace_back(GL_COLOR_ATTACHMENT0 + i);
        glDrawBuffers(static_cast<GLsizei>(arr.size()), arr.data());
    }
    glViewport(0, 0, w, h);
}

void Framebuffer::unbindTarget() const {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    glViewport(0, 0, WindowWidth, WindowHeight);
}

void Framebuffer::bindDepthTexture(GLuint number) const {
    assert(depthTexture != 0);
    glActiveTexture(GL_TEXTURE0 + number);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glActiveTexture(GL_TEXTURE0);
}

void Framebuffer::bindColorTexture(GLuint index, GLuint number) const {
    assert(index < colorTextures.size());
    glActiveTexture(GL_TEXTURE0 + number);
    glBindTexture(GL_TEXTURE_2D, colorTextures[index]);
    glActiveTexture(GL_TEXTURE0);
}

void Framebuffer::bindColorTextures(GLuint startNumber) const {
    for (size_t i = 0; i < colorTextures.size(); i++) {
        glActiveTexture(GL_TEXTURE0 + startNumber + static_cast<GLuint>(i));
        glBindTexture(GL_TEXTURE_2D, colorTextures[i]);
        glActiveTexture(GL_TEXTURE0);
    }
}

void Framebuffer::copyDepthTexture(Framebuffer& target) const {
    assert(w == target.w && h == target.h && depthTexture != 0 && target.depthTexture);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, id);
    glReadBuffer(GL_DEPTH_ATTACHMENT);
    GLint saved = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &saved);
    glBindTexture(GL_TEXTURE_2D, target.depthTexture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
    glBindTexture(GL_TEXTURE_2D, saved);
}

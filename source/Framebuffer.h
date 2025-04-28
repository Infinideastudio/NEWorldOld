#pragma once

#include "Definitions.h"

class Framebuffer {
public:
	Framebuffer() {}
	Framebuffer(int width, int height, int cnt, bool depth, bool shadow = false, bool bilinear = false);
	Framebuffer(Framebuffer const&) = delete;
	Framebuffer(Framebuffer&& from) noexcept : Framebuffer() {
		swap(*this, from);
	}
	Framebuffer& operator=(Framebuffer const&) = delete;
	Framebuffer& operator=(Framebuffer&& from) noexcept {
		swap(*this, from);
		return *this;
	}

	friend void swap(Framebuffer& first, Framebuffer& second) {
		using std::swap;
		swap(first.w, second.w);
		swap(first.h, second.h);
		swap(first.id, second.id);
		swap(first.depthTexture, second.depthTexture);
		swap(first.depthRenderbuffer, second.depthRenderbuffer);
		swap(first.colorTextures, second.colorTextures);
	}

	~Framebuffer() {
		if (id != 0) glDeleteFramebuffers(1, &id);
		if (depthTexture != 0) glDeleteTextures(1, &depthTexture);
		if (depthRenderbuffer != 0) glDeleteRenderbuffers(1, &depthRenderbuffer);
		glDeleteTextures(static_cast<GLsizei>(colorTextures.size()), colorTextures.data());
	}

	bool empty() const { return id == 0; }
	int width() const { return w; }
	int height() const { return h; }

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

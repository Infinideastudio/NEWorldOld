#pragma once

#include "Definitions.h"
#include "GLProc.h"

class Framebuffer {
public:
	Framebuffer() {}
	Framebuffer(int width, int height, int cnt, bool depth, bool shadow);
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
		swap(first.width, second.width);
		swap(first.height, second.height);
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

	void bindTarget() {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id);
		if (colorTextures.empty()) {
			glDrawBuffer(GL_NONE);
		}
		else {
			std::vector<GLuint> arr;
			for (int i = 0; i < colorTextures.size(); i++) arr.emplace_back(GL_COLOR_ATTACHMENT0 + i);
			glDrawBuffers(static_cast<GLsizei>(arr.size()), arr.data());
		}
		glViewport(0, 0, width, height);
	}

	void unbindTarget() {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glDrawBuffer(GL_BACK);
		glViewport(0, 0, windowwidth, windowheight);
	}

	void bindDepthTexture(GLuint number) {
		assert(depthTexture != 0);
		glActiveTexture(GL_TEXTURE0 + number);
		glBindTexture(GL_TEXTURE_2D, depthTexture);
		glActiveTexture(GL_TEXTURE0);
	}

	void bindColorTextures(GLuint startNumber) {
		for (size_t i = 0; i < colorTextures.size(); i++) {
			glActiveTexture(GL_TEXTURE0 + startNumber + static_cast<GLuint>(i));
			glBindTexture(GL_TEXTURE_2D, colorTextures[i]);
			glActiveTexture(GL_TEXTURE0);
		}
	}

	void copyDepthTexture(Framebuffer& target) {
		assert(width == target.width && height == target.height && depthTexture != 0 && target.depthTexture);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, id);
		glReadBuffer(GL_DEPTH_ATTACHMENT);
		GLint saved;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &saved);
		glBindTexture(GL_TEXTURE_2D, target.depthTexture);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
		glBindTexture(GL_TEXTURE_2D, saved);
	}

private:
	int width = 0;
	int height = 0;
	GLuint id = 0;
	GLuint depthTexture = 0;
	GLuint depthRenderbuffer = 0;
	std::vector<GLuint> colorTextures;
};

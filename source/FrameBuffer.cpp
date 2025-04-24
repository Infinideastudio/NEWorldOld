#include "FrameBuffer.h"

FrameBuffer::FrameBuffer(int size_, int cnt, bool depth, bool shadow) {
	create(size_, cnt, depth, shadow);
}

FrameBuffer::~FrameBuffer() {
	if (init) destroy();
}

void FrameBuffer::create(int size_, int cnt, bool depth, bool shadow) {
	if (init) destroy();

	size = size_;
	colorAttachCount = cnt;
	depthAttach = depth;

	// Create framebuffer object
	glGenFramebuffers(1, &id);
	glBindFramebuffer(GL_FRAMEBUFFER, id);

	if (depth) {
		// Create depth texture
		glGenTextures(1, &depthAttachment);
		glBindTexture(GL_TEXTURE_2D, depthAttachment);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		if (shadow) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		}
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		// Attach
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthAttachment, 0);
	} else {
		// Create depth renderbuffer
		glGenRenderbuffers(1, &depthAttachment);
		glBindRenderbuffer(GL_RENDERBUFFER, depthAttachment);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
		// Attach
		glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthAttachment);
	}

	// Create color textures
	glGenTextures(cnt, colorTexture);
	for (int i = 0; i < cnt; i++) {
		glBindTexture(GL_TEXTURE_2D, colorTexture[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
		// Attach
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorTexture[i], 0);
	}

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) DebugError("Framebuffer creation error!");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	init = true;
}

void FrameBuffer::destroy() {
	if (!init) return;

	glDeleteFramebuffers(1, &id);
	if (depthAttach) glDeleteTextures(1, &depthAttachment);
	else glDeleteRenderbuffers(1, &depthAttachment);
	glDeleteTextures(colorAttachCount, colorTexture);

	init = false;
}

void FrameBuffer::bindTargetBuffer(int index, int width, int height) {
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id);
	if (colorAttachCount == 0) {
		glDrawBuffer(GL_NONE);
	} else {
		GLuint arr = GL_COLOR_ATTACHMENT0 + index;
		glDrawBuffers(1, &arr);
	}
	if (width == -1) glViewport(0, 0, size, size);
	else glViewport(0, 0, width, height);
}

void FrameBuffer::bindTarget(int width, int height) {
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id);
	if (colorAttachCount == 0) {
		glDrawBuffer(GL_NONE);
	} else {
		std::vector<GLuint> arr(colorAttachCount);
		for (int i = 0; i < colorAttachCount; i++) arr[i] = GL_COLOR_ATTACHMENT0 + i;
		glDrawBuffers(colorAttachCount, arr.data());
	}
	if (width == -1) glViewport(0, 0, size, size);
	else glViewport(0, 0, width, height);
}

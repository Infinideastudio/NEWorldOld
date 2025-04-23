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
	glGenFramebuffersEXT(1, &id);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, id);

	if (depth) {
		// Create depth texture
		glGenTextures(1, &depthAttachment);
		glBindTexture(GL_TEXTURE_2D, depthAttachment);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_NONE);
		if (shadow) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
		}
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24_ARB, size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		// Attach
		glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, depthAttachment, 0);
	} else {
		// Create depth renderbuffer
		glGenRenderbuffersEXT(1, &depthAttachment);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depthAttachment);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24_ARB, size, size);
		// Attach
		glFramebufferRenderbufferEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depthAttachment);
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
		glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + i, GL_TEXTURE_2D, colorTexture[i], 0);
	}

	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT) DebugError("Framebuffer creation error!");
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	init = true;
}

void FrameBuffer::destroy() {
	if (!init) return;

	glDeleteFramebuffersEXT(1, &id);

	if (depthAttach) glDeleteTextures(1, &depthAttachment);
	else glDeleteRenderbuffersEXT(1, &depthAttachment);

	glDeleteTextures(colorAttachCount, colorTexture);

	init = false;
}

void FrameBuffer::bindTargetBuffer(int index, int width, int height) {
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, id);
	if (colorAttachCount == 0) {
		glDrawBuffer(GL_NONE);
	} else {
		GLuint arr = GL_COLOR_ATTACHMENT0_EXT + index;
		glDrawBuffersARB(1, &arr);
	}
	if (width == -1) glViewport(0, 0, size, size);
	else glViewport(0, 0, width, height);
}

void FrameBuffer::bindTarget(int width, int height) {
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, id);
	if (colorAttachCount == 0) {
		glDrawBuffer(GL_NONE);
	} else {
		std::vector<GLuint> arr(colorAttachCount);
		for (int i = 0; i < colorAttachCount; i++) arr[i] = GL_COLOR_ATTACHMENT0_EXT + i;
		glDrawBuffersARB(colorAttachCount, arr.data());
	}
	if (width == -1) glViewport(0, 0, size, size);
	else glViewport(0, 0, width, height);
}

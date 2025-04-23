#pragma once

#include "Definitions.h"
#include "GLProc.h"

class FrameBuffer {
public:
	FrameBuffer(): init(false) {}
	FrameBuffer(int size_, int cnt, bool depth, bool shadow);
	~FrameBuffer();

	void create(int size_, int cnt, bool depth, bool shadow);
	void destroy();

	void bindTargetBuffer(int index, int width = -1, int height = -1);
	void bindTarget(int width = -1, int height = -1);
	void unbindTarget() {
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
		glDrawBuffer(GL_BACK);
		glReadBuffer(GL_BACK);
		glViewport(0, 0, windowwidth, windowheight);
	}

	void bindColorTextures(int startNumber) {
		for (int i = 0; i < colorAttachCount; i++) {
			glActiveTextureARB(GL_TEXTURE0 + startNumber + i);
			glBindTexture(GL_TEXTURE_2D, colorTexture[i]);
			glActiveTextureARB(GL_TEXTURE0);
		}
	}

	void bindDepthTexture(int number) {
		assert(depthAttach);
		glActiveTextureARB(GL_TEXTURE0 + number);
		glBindTexture(GL_TEXTURE_2D, depthAttachment);
		glActiveTextureARB(GL_TEXTURE0);
	}

	void copyDepthTexture(FrameBuffer& target) {
		assert(depthAttach && target.depthAttach && size == target.size);
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, id);
		glReadBuffer(GL_DEPTH_ATTACHMENT_EXT);
		GLint saved;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &saved);
		glBindTexture(GL_TEXTURE_2D, target.depthAttachment);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, size, size);
		glBindTexture(GL_TEXTURE_2D, saved);
	}

private:
	int size, colorAttachCount;
	bool init, depthAttach;
	
	GLuint id, colorTexture[16], depthAttachment;
};

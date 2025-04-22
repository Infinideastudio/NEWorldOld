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
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);
		glDrawBuffer(GL_BACK);
		glReadBuffer(GL_BACK);
		glViewport(0, 0, windowwidth, windowheight);
	}

	void bindColorTextures(int startNumber) {
		for (int i = 0; i < colorAttachCount; i++) {
			glActiveTextureARB(GL_TEXTURE0 + startNumber + i);
			glBindTexture(GL_TEXTURE_2D, colorTexture[i]);
		}
		glActiveTextureARB(GL_TEXTURE0);
	}

	void bindDepthTexture(int number) {
		assert(depthAttach);
		glActiveTextureARB(GL_TEXTURE0 + number);
		glBindTexture(GL_TEXTURE_2D, depthAttachment);
		glActiveTextureARB(GL_TEXTURE0);
	}

private:
	int size, colorAttachCount;
	bool init, depthAttach;
	
	GLuint id, colorTexture[16], depthAttachment;
};

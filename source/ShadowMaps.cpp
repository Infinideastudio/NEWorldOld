#include "ShadowMaps.h"
#include "Particles.h"
#include "Mat4.h"

namespace ShadowMaps {

	void BuildShadowMap(double xpos, double ypos, double zpos, double heading, double pitch, const FrustumTest& playerFrustum, double curtime, float gameTime) {
		int cx = getchunkpos((int)xpos), cy = getchunkpos((int)ypos), cz = getchunkpos((int)zpos);

		Renderer::StartShadowPass(gameTime);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		//glDisable(GL_TEXTURE_2D);
		glDisable(GL_FOG);
		glDisable(GL_BLEND);
		FrustumTest frus = Renderer::getShadowMapFrustum(xpos, ypos, zpos, heading, pitch, playerFrustum);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glMultMatrixf(frus.getProjMatrix());
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glMultMatrixf(frus.getModlMatrix());

		WorldRenderer::ListRenderChunks(cx, cy, cz, Renderer::shadowdist + 2, curtime, false);

		MutexUnlock(Mutex);
		glClear(GL_DEPTH_BUFFER_BIT);
		//glDisable(GL_CULL_FACE);
		//glCullFace(GL_FRONT);
		WorldRenderer::RenderChunks(xpos, ypos, zpos, 0);
		//glEnable(GL_CULL_FACE);
		//glCullFace(GL_BACK);
		MutexLock(Mutex);

		Particles::renderall(xpos, ypos, zpos);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		Renderer::EndShadowPass();

		glEnable(GL_FOG);
		glEnable(GL_BLEND);
	}

	/*
	void RenderShadowMap(double xpos, double ypos, double zpos, double curtime) {
	    int cx = getchunkpos((int)xpos), cy = getchunkpos((int)ypos), cz = getchunkpos((int)zpos);

	    Renderer::bindShader(Renderer::DepthShader);
	    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	    glEnableClientState(GL_VERTEX_ARRAY);
	    glDisable(GL_TEXTURE_2D);
	    glDisable(GL_FOG);
	    glDisable(GL_BLEND);
	    float scale = 16.0f;
	    float length = Renderer::shadowdist*scale;
	    glMatrixMode(GL_PROJECTION);
	    glLoadIdentity();
	    glOrtho(-length, length, -length, length, -length, length);
	    glMatrixMode(GL_MODELVIEW);
	    glLoadIdentity();
	    glRotated(Renderer::sunlightXrot, 1.0, 0.0, 0.0);
	    glRotated(Renderer::sunlightYrot, 0.0, 1.0, 0.0);

	    WorldRenderer::ListRenderChunks(cx, cy, cz, Renderer::shadowdist + 1, curtime, false);
	    MutexUnlock(Mutex);
	    WorldRenderer::RenderChunks(xpos, ypos, zpos, 3);
	    MutexLock(Mutex);

	    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	    glDisableClientState(GL_VERTEX_ARRAY);
	    Shader::unbind();

	    glEnable(GL_FOG);
	    glEnable(GL_BLEND);
	}
	*/

	void DrawShadowMap(int xi, int yi, int xa, int ya) {
		glDisable(GL_TEXTURE_2D);
		glColor4f(0.2f, 0.2f, 0.2f, 0.5f);
		glBegin(GL_QUADS);
		glVertex2i(xi, ya);
		glVertex2i(xa, ya);
		glVertex2i(xa, yi);
		glVertex2i(xi, yi);
		glEnd();

		glEnable(GL_TEXTURE_2D);
		Renderer::shadow.bindDepthTexture(0);
		Renderer::shaders[Renderer::ShowDepthShader].bind();
		Renderer::shaders[Renderer::ShowDepthShader].setUniform("Tex", 0);

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f);
		glVertex2i(xi, ya);
		glTexCoord2f(1.0f, 0.0f);
		glVertex2i(xa, ya);
		glTexCoord2f(1.0f, 1.0f);
		glVertex2i(xa, yi);
		glTexCoord2f(0.0f, 1.0f);
		glVertex2i(xi, yi);
		glEnd();

		Shader::unbind();
	}
}
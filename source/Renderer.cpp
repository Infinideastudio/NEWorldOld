#include "Renderer.h"
namespace Renderer {
	
	int Vertexes, Texcoordc, Colorc;
	float* VertexArray = nullptr;
	float* VA = nullptr;
	float tc[3], col[4];
	//unsigned int Buffers[3];
	bool AdvancedRender;
	int ShadowRes = 4096;
	int MaxShadowDist = 8;
	int shadowdist;
	float sunlightXrot, sunlightYrot;
	GLhandleARB shaders[16];
	GLhandleARB shaderPrograms[16];
	int shadercount = 0, ActiveShader = -1;
	int index = 0, size = 0;
	unsigned int ShadowFBO, DepthTexture;
	
	void Init(int tcc, int cc) {
		Texcoordc = tcc; Colorc = cc;
		if (VertexArray == nullptr) VertexArray = new float[ArraySize];
		index = 0;
		VA = VertexArray;
		Vertexes = 0;
		size = (tcc + cc + 3) * 4;
	}

	void Vertex3f(float x, float y, float z) {
		if ((Vertexes + 1)*(Texcoordc + Colorc + 3) > ArraySize) return;
		if (Texcoordc != 0) for (int i = 0; i < Texcoordc; i++) VertexArray[index++] = tc[i];
		if (Colorc != 0) for (int i = 0; i < Colorc; i++) VertexArray[index++] = col[i];
		VertexArray[index++] = x;
		VertexArray[index++] = y;
		VertexArray[index++] = z;
		Vertexes++;
	}

	void TexCoord2f(float x, float y) { tc[0] = x; tc[1] = y; }
	void TexCoord3f(float x, float y, float z) { tc[0] = x; tc[1] = y; tc[2] = z; }
	void Color3f(float r, float g, float b) { col[0] = r; col[1] = g; col[2] = b; }
	void Color4f(float r, float g, float b, float a) { col[0] = r; col[1] = g; col[2] = b; col[3] = a; }
	
	void Flush(VBOID& buffer, vtxCount& vtxs) {

		//上次才知道原来Flush还有冲厕所的意思QAQ
		//OpenGL有个函数glFlush()，翻译过来就是GL冲厕所() ←_←

		vtxs = Vertexes;
		if (Vertexes != 0) {
			if (buffer == 0) glGenBuffersARB(1, &buffer);
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
			glBufferDataARB(GL_ARRAY_BUFFER_ARB,
				Vertexes * ((Texcoordc + Colorc + 3) * sizeof(float)),
				VertexArray, GL_STATIC_DRAW_ARB);
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		}
	}

	void renderbuffer(VBOID buffer, vtxCount vtxs, int tc, int cc) {

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
		int cnt = tc + cc + 3;
		if (tc != 0) {
			if (cc != 0) {
				glTexCoordPointer(tc, GL_FLOAT, cnt * sizeof(float), (float*)0);
				glColorPointer(cc, GL_FLOAT, cnt * sizeof(float), (float*)(tc * sizeof(float)));
				glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)((tc + cc) * sizeof(float)));
			}
			else {
				glTexCoordPointer(tc, GL_FLOAT, cnt * sizeof(float), (float*)0);
				glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)(tc * sizeof(float)));
			}
		}
		else {
			if (cc != 0) {
				glColorPointer(cc, GL_FLOAT, cnt * sizeof(float), (float*)0);
				glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)(cc * sizeof(float)));
			}
			else glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)0);
		}
		
		//这个框是不是很装逼2333 --qiaozhanrong
		//====================================================================================================//
		/**/																								/**/
		/**/																								/**/
		/**/								glDrawArrays(GL_QUADS, 0, vtxs);								/**/
		/**/																								/**/
		/**/																								/**/
		//====================================================================================================//

	}

	void initShaders() {
		sunlightXrot = 30.0f;
		sunlightYrot = 60.0f;

		shadercount = 3;
		shaders[0] = loadShader("Shaders/Main.vsh", GL_VERTEX_SHADER_ARB);
		shaders[1] = loadShader("Shaders/Main.fsh", GL_FRAGMENT_SHADER_ARB);
		shaders[2] = loadShader("Shaders/MergeSurface.vsh", GL_VERTEX_SHADER_ARB);
		shaders[3] = loadShader("Shaders/MergeSurface.fsh", GL_FRAGMENT_SHADER_ARB);
		shaders[4] = loadShader("Shaders/Shadow.vsh", GL_VERTEX_SHADER_ARB);
		shaders[5] = loadShader("Shaders/Shadow.fsh", GL_FRAGMENT_SHADER_ARB);
		for (int i = 0; i != shadercount; i++) {
			shaderPrograms[i] = glCreateProgramObjectARB();
			glAttachObjectARB(shaderPrograms[i], shaders[i * 2]);
			glAttachObjectARB(shaderPrograms[i], shaders[i * 2 + 1]);
			glLinkProgramARB(shaderPrograms[i]);
		}

		glGenTextures(1, &DepthTexture);
		glBindTexture(GL_TEXTURE_2D, DepthTexture);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, ShadowRes, ShadowRes, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);

		glActiveTextureARB(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, DepthTexture);
		glActiveTextureARB(GL_TEXTURE0);

		glGenFramebuffersEXT(1, &ShadowFBO);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, ShadowFBO);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, DepthTexture, 0);
		if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT) {
			DebugError("Frame buffer creation error!");
		}
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		
		glUseProgramObjectARB(shaderPrograms[0]);
		glUniform1iARB(glGetUniformLocationARB(shaderPrograms[0], "Tex"), 0);
		glUniform1iARB(glGetUniformLocationARB(shaderPrograms[0], "DepthTex"), 1);
		glUseProgramObjectARB(0);
		
	}

	void destroyShaders() {
		for (int i = 0; i != shadercount; i++) {
			glDetachObjectARB(shaderPrograms[i], shaders[i * 2]);
			glDetachObjectARB(shaderPrograms[i], shaders[i * 2 + 1]);
			glDeleteObjectARB(shaders[i * 2]);
			glDeleteObjectARB(shaders[i * 2 + 1]);
			glDeleteObjectARB(shaderPrograms[i]);
		}
		glDeleteTextures(1, &DepthTexture);
		//glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &ShadowFBO);
	}

	GLhandleARB loadShader(string filename, unsigned int mode) {
		GLhandleARB res;
		string cur;
		int lines = 0, curlen;;
		char* curline;
		std::vector<char*> source;
		std::vector<int> length;
		std::ifstream filein(filename);
		if (!filein.is_open()) return NULL;
		while (!filein.eof()) {
			lines++;
			std::getline(filein, cur);
			cur += '\n';
			curlen = cur.size();
			curline = new char[curlen];
			memcpy(curline, cur.c_str(), curlen);
			source.push_back(curline);
			length.push_back(curlen);
		}
		filein.close();
		res = glCreateShaderObjectARB(mode);
		glShaderSourceARB(res, lines, (const GLchar**)source.data(), length.data());
		glCompileShaderARB(res);
		for (int i = 0; i < lines; i++) delete[] source[i];
		int st = GL_TRUE;
		glGetObjectParameterivARB(res, GL_COMPILE_STATUS, &st);
		if (st == GL_FALSE) printInfoLog(res);
		return res;
	}

	void printInfoLog(GLhandleARB obj) {
		int infologLength, charsWritten;
		char* infoLog;
		glGetObjectParameterivARB(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB, &infologLength);
		if (infologLength != 0) {
			infoLog = new char[infologLength];
			glGetInfoLogARB(obj, infologLength, &charsWritten, infoLog);
			cout << infoLog << endl;
			delete[] infoLog;
		}
	}

	void EnableShaders() {
		shadowdist = min(MaxShadowDist, viewdistance);
		if (MergeFace) ActiveShader = 1; else ActiveShader = 0;

		glUseProgramObjectARB(shaderPrograms[ActiveShader]);
		glUniform1fARB(glGetUniformLocationARB(shaderPrograms[ActiveShader], "renderdist"), viewdistance * 16.0f);

		//Calc matrix
		float scale = 16.0f * sqrt(3.0f);
		float length = shadowdist*scale;
		Frustum::LoadIdentity();
		Frustum::SetOrtho(-length, length, -length, length, -length, length);
		Frustum::MultRotate(sunlightXrot, 1.0f, 0.0f, 0.0f);
		Frustum::MultRotate(sunlightYrot, 0.0f, 1.0f, 0.0f);

		//Set matrix uniform
		glUniformMatrix4fvARB(glGetUniformLocationARB(shaderPrograms[ActiveShader], "Depth_proj"), 1, GL_FALSE, Frustum::proj);
		glUniformMatrix4fvARB(glGetUniformLocationARB(shaderPrograms[ActiveShader], "Depth_modl"), 1, GL_FALSE, Frustum::modl);
	}

	void DisableShaders() {
		glUseProgramObjectARB(0);
	}

	void StartShadowPass() {
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, ShadowFBO);
		glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
		ActiveShader = 2;
		glUseProgramObjectARB(shaderPrograms[2]);
		glViewport(0, 0, ShadowRes, ShadowRes);
	}

	void EndShadowPass() {
		glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDrawBuffer(GL_BACK); glReadBuffer(GL_BACK);
		ActiveShader = -1;
		glUseProgramObjectARB(0);
		glViewport(0, 0, windowwidth, windowheight);
	}
}
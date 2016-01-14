#include "Renderer.h"
#include <time.h>
namespace Renderer {
	
	int Vertexes, Texcoordc, Colorc;
	float* VertexArray = nullptr;
	float* VA = nullptr;
	float tc[3], col[4];
	unsigned int Buffers[3];
	bool ShaderAval, UseShaders = false;
	GLhandleARB shaders[16];
	GLhandleARB shaderPrograms[16];
	int shadercount = 0;
	int index = 0, size = 0;

	void Init(int tcc, int cc) {
		Texcoordc = tcc; Colorc = cc;
		if (VertexArray == nullptr) VertexArray = new float[ArrayUNITSIZE];
		index = 0; 
		VA = VertexArray;
		Vertexes = 0;
		size = (tcc + cc + 3) * 4;
	}

	void Vertex3f(float x, float y, float z) {
		if ((Vertexes + 1)*(Texcoordc + Colorc + 3) > ArrayUNITSIZE) return;
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

		/*
		if (EnableShaders) {
			for (int i = 0; i < shadercount; i++) {
				glUseProgramObjectARB(shaderPrograms[i]);
			}
		}
		*/

		vtxs = Vertexes;
		if (Vertexes != 0) {
			if (buffer == 0) glGenBuffersARB(1, &buffer);
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
			glBufferDataARB(GL_ARRAY_BUFFER_ARB, Vertexes * (Texcoordc + Colorc + 3) * sizeof(float), VertexArray, GL_STATIC_DRAW_ARB);
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		}

		//if (EnableShaders) glUseProgramObjectARB(0);
	}
    
	void renderbuffer(VBOID buffer, vtxCount vtxs, int ctex, int ccol) {

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
		int cnt = ctex + ccol + 3;

		if (ctex != 0) {
			if (ccol != 0) {
				glTexCoordPointer(ctex, GL_FLOAT, cnt * sizeof(float), (float*)0);
				glColorPointer(ccol, GL_FLOAT, cnt * sizeof(float), (float*)(ctex * sizeof(float)));
				glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)((ctex + ccol) * sizeof(float)));
			}
			else {
				glTexCoordPointer(ctex, GL_FLOAT, cnt * sizeof(float), (float*)0);
				glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)(ctex * sizeof(float)));
			}
		}
		else {
			if (ccol != 0) {
				glColorPointer(ccol, GL_FLOAT, cnt * sizeof(float), (float*)0);
				glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)(ccol * sizeof(float)));
			}
			else {
				glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)0);
			}
		}

		//================================
		glDrawArrays(GL_QUADS, 0, vtxs);
		//================================

	}

	void initShaders() {
		shadercount = 2;
		shaders[0] = loadShader("Shaders/Main.vsh", GL_VERTEX_SHADER_ARB);
		shaders[1] = loadShader("Shaders/Main.fsh", GL_FRAGMENT_SHADER_ARB);
		shaders[2] = loadShader("Shaders/MergeSurface.vsh", GL_VERTEX_SHADER_ARB);
		shaders[3] = loadShader("Shaders/MergeSurface.fsh", GL_FRAGMENT_SHADER_ARB);
		for (int i = 0; i != shadercount; i++) {
			shaderPrograms[i] = glCreateProgramObjectARB();
			glAttachObjectARB(shaderPrograms[i], shaders[i * 2]);
			glAttachObjectARB(shaderPrograms[i], shaders[i * 2 + 1]);
			glLinkProgramARB(shaderPrograms[i]);
		}
	}

	GLhandleARB loadShader(string filename, unsigned int mode) {
		GLhandleARB res;
		string cur;
		int lines = 0, curlen;;
		char* curline;
		const char* curline_c;
		std::vector<char*> source;
		std::vector<const char*> source_c;
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
			curline_c = curline;
			source_c.push_back(curline_c);
			length.push_back(curlen);
		}
		filein.close();
		res = glCreateShaderObjectARB(mode);
		glShaderSourceARB(res, lines, source_c.data(), length.data());
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
		if (MergeFace) {
			glUseProgramObjectARB(shaderPrograms[1]);
			glUniform1fARB(glGetUniformLocationARB(shaderPrograms[1], "texwidth"), 1 / 8);
		}
		else {
			glUseProgramObjectARB(shaderPrograms[0]);
			glUniform1iARB(glGetUniformLocationARB(shaderPrograms[0], "renderdist"), viewdistance);
		}
	}

	void DisableShaders() {
		glUseProgramObjectARB(0);
	}
}
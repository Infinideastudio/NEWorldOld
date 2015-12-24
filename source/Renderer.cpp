#include "Renderer.h"
namespace renderer {
	
	int Vertexes, Texcoordc, Colorc;
	float* VertexArray = nullptr;
	float tc[3], col[4];
	unsigned int Buffers[3];
	bool ShaderAval, EnableShaders = false;
	GLhandleARB shaders[16];
	GLhandleARB shaderPrograms[16];
	int shadercount = 0;
	int index = 0;

	void Init(int tcc, int cc) {
		Texcoordc = tcc; Colorc = cc;
		if (VertexArray == nullptr) VertexArray = new float[ArrayUNITSIZE];
		index = 0;
		Vertexes = 0;
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
    
	//GLhandleARB loadShader(string filename, uint mode){
        
		//GLhandleARB shader;
  //      dim as zstring ptr ptr shaderSource
  //      dim as integer ptr shaderSourceLen
  //      dim as integer shaderLineNums
  //      dim shaderSourceCurLine as zstring ptr
  //      dim shaderSourceCurLineStr as string
  //      dim f as integer
  //      f=freefile
  //      open filename for input as #f
  //      do until eof(f)
  //          line input #f,shaderSourceCurLineStr
  //          print shaderSourceCurLineStr
  //          shaderSourceCurLine=allocate(len(shaderSourceCurLineStr)+2)
  //          *shaderSourceCurLine=shaderSourceCurLineStr+chr(10)
  //          shaderlinenums+=1
  //          shaderSource=reallocate(shaderSource,shaderlinenums*sizeof(zstring ptr))
  //          shaderSource[shaderlinenums-1]=shaderSourceCurLine
  //          print *shaderSource[shaderlinenums-1]
  //          shaderSourceLen=reallocate(shaderSourceLen,shaderlinenums*sizeof(integer))
  //          shaderSourceLen[shaderlinenums-1]=len(shaderSourceCurLineStr)+1
  //      loop
  //      close #f
  //      shader=glCreateShaderObjectARB(mode)
  //      glShaderSourceARB(shader,shaderLineNums,shaderSource,0)
  //      glCompileShaderARB(shader)
  //      return shader;
        
    //}
    
    void initShader(){

		for (int i = 0; i != shadercount; i++) {
			shaderPrograms[i] = glCreateProgramObjectARB();
			glAttachObjectARB(shaderPrograms[i*2], shaders[i]);
			glAttachObjectARB(shaderPrograms[i*2+1], shaders[i]);
			glLinkProgramARB(shaderPrograms[i]);
		}

    }
}
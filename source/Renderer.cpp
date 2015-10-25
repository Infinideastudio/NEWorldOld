#include "Renderer.h"
namespace renderer {

	bool Textured, Colored;
	int Vertexes;
	float* VertexArray = nullptr;
	float u, v, r, g, b;
	unsigned int Buffers[3];
	bool ShaderAval, EnableShaders = false;
	GLhandleARB shaders[16];
	GLhandleARB shaderPrograms[16];
	int shadercount = 0;
	int index = 0;

	void Init() {
		if (VertexArray == nullptr){
			VertexArray = new float[ArrayUNITSIZE];
		}
		index = 0;
		Vertexes = 0;
		Textured = false;
		Colored = false;
	}

	void Vertex3d(double x, double y, double z){
		Vertex3f((float)x, (float)y, (float)z);
	}

	void TexCoord2d(double x, double y){
		TexCoord2f((float)x, (float)y);
	}

	void Color3d(double _r, double _g, double _b){
		Color3f((float)_r, (float)_g, (float)_b);
	}

	void Vertex3f(float x, float y, float z) {
		if (Textured) {
			VertexArray[index++] = u;
			VertexArray[index++] = v;
		}

		if (Colored) {
			VertexArray[index++] = r;
			VertexArray[index++] = g;
			VertexArray[index++] = b;
		}

		VertexArray[index++] = x;
		VertexArray[index++] = y;
		VertexArray[index++] = z;

		Vertexes++;
	}

	void TexCoord2f(float u_, float v_) {
		u = u_;
		v = v_;
		Textured = true;
	}

	void Color3f(float r_, float g_, float b_) {
		r = r_;
		g = g_;
		b = b_;
		Colored = true;
	}

	void Flush(VBOID& buffer, vtxCount& vtxs) {

		//上次才知道原来Flush还有冲厕所的意思QAQ
		//OpenGL有个函数glFlush()，翻译过来就是GL冲厕所() ←_←

		vtxs = Vertexes;

		/*
		if (EnableShaders) {
			for (int i = 0; i < shadercount; i++) {
				glUseProgramObjectARB(shaderPrograms[i]);
			}
		}
		*/

		if (Vertexes > 0) {

			if (buffer == 0)glGenBuffersARB(1, &buffer);
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);

			if (Textured) {
				if (Colored)
					glBufferDataARB(GL_ARRAY_BUFFER_ARB, Vertexes * 8 * sizeof(float), VertexArray, GL_STATIC_DRAW_ARB);
				else
					glBufferDataARB(GL_ARRAY_BUFFER_ARB, Vertexes * 5 * sizeof(float), VertexArray, GL_STATIC_DRAW_ARB);
			}
			else {
				if (Colored)
					glBufferDataARB(GL_ARRAY_BUFFER_ARB, Vertexes * 6 * sizeof(float), VertexArray, GL_STATIC_DRAW_ARB);
				else
					glBufferDataARB(GL_ARRAY_BUFFER_ARB, Vertexes * 3 * sizeof(float), VertexArray, GL_STATIC_DRAW_ARB);
			}

			//重置
			Init();
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

		}

		//if (EnableShaders) glUseProgramObjectARB(0);

	}
    
	void renderbuffer(VBOID buffer, vtxCount vtxs, bool ftex, bool fcol) {

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);

		if (ftex) {
			if (fcol) {
				glTexCoordPointer(2, GL_FLOAT, 8 * sizeof(float), (float*)0);
				glColorPointer(3, GL_FLOAT, 8 * sizeof(float), (float*)(2 * sizeof(float)));
				glVertexPointer(3, GL_FLOAT, 8 * sizeof(float), (float*)(5 * sizeof(float)));
			}
			else {
				glTexCoordPointer(2, GL_FLOAT, 5 * sizeof(float), (float*)0);
				glVertexPointer(3, GL_FLOAT, 5 * sizeof(float), (float*)(2 * sizeof(float)));
			}
		}
		else {
			if (fcol) {
				glColorPointer(3, GL_FLOAT, 6 * sizeof(float), (float*)0);
				glVertexPointer(3, GL_FLOAT, 6 * sizeof(float), (float*)(3 * sizeof(float)));
			}
			else {
				glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), (float*)0);
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
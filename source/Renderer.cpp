#include "Renderer.h"

namespace Renderer {
	
	/*
	好纠结啊好纠结，“高级”渲染模式里的所有数据要不要都用VertexAttribArray啊。。。
	然而我还是比较懒。。。所以除了【附加】的顶点属性之外，其他属性（比如颜色、纹理坐标）都保留原来的算了。。。
	
	说到为啥要用【附加】的顶点属性。。。这是由于Shadow Map的精度问题。。。
	有的时候背光面的外圈会有亮光。。。很难看。。。所以要用Shader把背光面弄暗。。。
	于是如何让shader知道这个面朝哪里呢？懒得用NormalArray的我就用了一个附加的顶点属性。。。
	0.0f表示前面(z+)，1.0f表示后面(z-)，2.0f表示右面(x+)，3.0f表示左面(x-)，4.0f表示上面(y+)，5.0f表示下面(y-)

		你没有看错。。。这些值。。。全都是

			浮！
				点！
					型！
						的！！！！！！！

	坑爹的GLSL不支持整型作为顶点属性。。。只好用浮点型代替了(╯‵□′)╯︵┻━┻
	然后为了解决浮点数的精度问题，我在shader里写了个四舍五入取整。。。
	不说了。。。

	等等我还没有签名呢。。。
	--qiaozhanrong

	====================================================
	留言板：

	1楼. qiaozhanrong: 自己抢个沙发先

	2楼. [请输入姓名]: [请输入回复内容]

	[回复]
	====================================================
	*/

	int Vertexes, Texcoordc, Colorc, Attribc;
	float* VertexArray = nullptr;
	float* VA = nullptr;
	float TexCoords[3], Colors[4], Attribs;
	//unsigned int Buffers[3];
	bool AdvancedRender;
	int ShadowRes = 4096;
	int MaxShadowDist = 2;
	int shadowdist;
	float sunlightXrot, sunlightYrot;
	GLhandleARB shaders[16];
	GLhandleARB shaderPrograms[16];
	int ActiveShader;
	int shadercount = 0;
	int index = 0, size = 0;
	unsigned int ShadowFBO, DepthTexture;
	unsigned int ShaderAttribLoc = 0;
	
	void Init(int tc, int cc, int ac) {
		Texcoordc = tc; Colorc = cc; Attribc = ac;
		if (VertexArray == nullptr) VertexArray = new float[ArraySize];
		index = 0;
		VA = VertexArray;
		Vertexes = 0;
		size = (tc + cc + + ac + 3) * 4;
	}

	void Vertex3f(float x, float y, float z) {
		if ((Vertexes + 1)*(Texcoordc + Colorc + 3) > ArraySize) return;
		if (Attribc != 0) VertexArray[index++] = Attribs;
		if (Texcoordc != 0) memcpy(VertexArray + index, TexCoords, Texcoordc*sizeof(float));
		index += Texcoordc;
		if (Colorc != 0) memcpy(VertexArray + index, Colors, Colorc*sizeof(float));
		index += Colorc;
		VertexArray[index++] = x;
		VertexArray[index++] = y;
		VertexArray[index++] = z;
		Vertexes++;
	}

	void TexCoord2f(float x, float y) { TexCoords[0] = x; TexCoords[1] = y; }
	void TexCoord3f(float x, float y, float z) { TexCoords[0] = x; TexCoords[1] = y; TexCoords[2] = z; }
	void Color3f(float r, float g, float b) { Colors[0] = r; Colors[1] = g; Colors[2] = b; }
	void Color4f(float r, float g, float b, float a) { Colors[0] = r; Colors[1] = g; Colors[2] = b; Colors[3] = a; }
	void Attrib1f(float a) { Attribs = a; }
	
	void Flush(VBOID& buffer, vtxCount& vtxs) {

		//上次才知道原来Flush还有冲厕所的意思QAQ
		//OpenGL有个函数glFlush()，翻译过来就是GL冲厕所() ←_←

		vtxs = Vertexes;
		if (Vertexes != 0) {
			if (buffer == 0) glGenBuffersARB(1, &buffer);
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
			glBufferDataARB(GL_ARRAY_BUFFER_ARB,
				Vertexes * ((Texcoordc + Colorc + Attribc + 3) * sizeof(float)),
				VertexArray, GL_STATIC_DRAW_ARB);
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		}
	}

	void renderbuffer(VBOID buffer, vtxCount vtxs, int tc, int cc, int ac) {

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
		int cnt = tc + cc + 3;
		if (!AdvancedRender || ac == 0) {
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
		}
		else {
			cnt += ac;
			glVertexAttribPointerARB(ShaderAttribLoc, ac, GL_FLOAT, GL_FALSE, cnt * sizeof(float), (float*)0);
			glTexCoordPointer(tc, GL_FLOAT, cnt * sizeof(float), (float*)(ac * sizeof(float)));
			glColorPointer(cc, GL_FLOAT, cnt * sizeof(float), (float*)((ac + tc) * sizeof(float)));
			glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)((ac + tc + cc) * sizeof(float)));
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
		ShaderAttribLoc = 1;
		std::set<string> defines;
		defines.insert("MergeFace");

		sunlightXrot = 30.0f;
		sunlightYrot = 60.0f;
		shadowdist = min(MaxShadowDist, viewdistance);

		shadercount = 4;
		shaders[0] = loadShader("Shaders/Main.vsh", GL_VERTEX_SHADER_ARB);
		shaders[1] = loadShader("Shaders/Main.fsh", GL_FRAGMENT_SHADER_ARB);
		shaders[2] = loadShader("Shaders/Main.vsh", GL_VERTEX_SHADER_ARB, &defines);
		shaders[3] = loadShader("Shaders/Main.fsh", GL_FRAGMENT_SHADER_ARB, &defines);
		shaders[4] = loadShader("Shaders/Shadow.vsh", GL_VERTEX_SHADER_ARB);
		shaders[5] = loadShader("Shaders/Shadow.fsh", GL_FRAGMENT_SHADER_ARB);
		shaders[6] = loadShader("Shaders/Depth.vsh", GL_VERTEX_SHADER_ARB);
		shaders[7] = loadShader("Shaders/Depth.fsh", GL_FRAGMENT_SHADER_ARB);
		for (int i = 0; i != shadercount; i++) {
			shaderPrograms[i] = glCreateProgramObjectARB();
			glAttachObjectARB(shaderPrograms[i], shaders[i * 2]);
			glAttachObjectARB(shaderPrograms[i], shaders[i * 2 + 1]);
			if (i == MainShader || i == MergeFaceShader) {
				glBindAttribLocationARB(shaderPrograms[i], ShaderAttribLoc, "VertexAttrib");
			}
			glLinkProgramARB(shaderPrograms[i]);
			int st = GL_TRUE;
			glGetObjectParameterivARB(shaderPrograms[i], GL_LINK_STATUS, &st);
			printInfoLog(shaderPrograms[i]);
			if (st == GL_FALSE) DebugWarning("Shader linking error!");
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
		
		for (int i = 0; i < 2; i++) {
			bindShader(i);
			if (i == 0) setUniform1i("Tex", 0);
			else setUniform1i("Tex3D", 0);
			setUniform1i("DepthTex", 1);
			setUniform4f("SkyColor", skycolorR, skycolorG, skycolorB, 1.0f);
		}
		unbindShader();
		
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

	GLhandleARB loadShader(string filename, unsigned int mode, std::set<string>* defines) {
		std::stringstream ss;
		GLhandleARB res;
		string cur, var, macro;
		int lines = 0, curlen;
		char* curline;
		std::vector<char*> source;
		std::vector<int> length;
		std::ifstream filein(filename);
		if (!filein.is_open()) return 0;
		while (!filein.eof()) {
			std::getline(filein, cur);
			if (!cur.size()) continue;
			ss.str(cur);
			macro = "";
			ss >> macro;
			if (macro == "##NEWORLD_SHADER_DEFINES") {
				ss >> var >> macro;
				if (defines != nullptr) {
					std::set<string>::iterator it = defines->find(var);
					if (it != defines->end()) cur = "#define " + macro;
					else continue;
				}
				else continue;
			}
			cur += '\n';
			curlen = cur.size();
			curline = new char[curlen];
			memcpy(curline, cur.c_str(), curlen);
			lines++;
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
		printInfoLog(res);
		if (st == GL_FALSE) DebugWarning("Shader compilation error! In file " + filename);
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

		//Enable shader
		bindShader(MergeFace ? MergeFaceShader : MainShader);

		//Calc matrix
		float scale = 16.0f * sqrt(3.0f);
		float length = shadowdist*scale;
		Frustum frus;
		frus.LoadIdentity();
		frus.SetOrtho(-length, length, -length, length, -length, length);
		frus.MultRotate(sunlightXrot, 1.0f, 0.0f, 0.0f);
		frus.MultRotate(sunlightYrot, 0.0f, 1.0f, 0.0f);

		//Set uniform
		setUniform1f("renderdist", viewdistance * 16.0f);
		setUniformMatrix4fv("Depth_proj", frus.getProjMatrix());
		setUniformMatrix4fv("Depth_modl", frus.getModlMatrix());
		setUniform1f("daylight", daylight);
		
		//Enable arrays for additional vertex attributes
		glEnableVertexAttribArrayARB(ShaderAttribLoc);
	}

	void DisableShaders() {
		//Disable shader
		unbindShader();

		//Disable arrays for additional vertex attributes
		glDisableVertexAttribArrayARB(ShaderAttribLoc);
	}

	void StartShadowPass() {
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, ShadowFBO);
		glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
		bindShader(ShadowShader);
		glViewport(0, 0, ShadowRes, ShadowRes);
	}

	void EndShadowPass() {
		glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDrawBuffer(GL_BACK); glReadBuffer(GL_BACK);
		unbindShader();
		glViewport(0, 0, windowwidth, windowheight);
	}

	bool setUniform1f(const char * uniform, float value) {
		int loc = getUniformLocation(uniform);
		assert(loc != -1);
		if (loc == -1) return false;
		glUniform1fARB(loc, value);
		return true;
	}

	bool setUniform1i(const char * uniform, int value) {
		int loc = getUniformLocation(uniform);
		assert(loc != -1);
		if (loc == -1) return false;
		glUniform1iARB(loc, value);
		return true;
	}

	bool setUniform4f(const char * uniform, float v0, float v1, float v2, float v3) {
		int loc = getUniformLocation(uniform);
		assert(loc != -1);
		if (loc == -1) return false;
		glUniform4fARB(loc, v0, v1, v2, v3);
		return true;
	}

	bool setUniformMatrix4fv(const char * uniform, float * value) {
		int loc = getUniformLocation(uniform);
		assert(loc != -1);
		if (loc == -1) return false;
		glUniformMatrix4fvARB(loc, 1, GL_FALSE, value);
		return true;
	}
}
#include "Renderer.h"
#include "Shader.h"

namespace Renderer {
	
	/*
	�þ��ᰡ�þ��ᣬ���߼�����Ⱦģʽ�����������Ҫ��Ҫ����VertexAttribArray��������
	Ȼ���һ��ǱȽ������������Գ��ˡ����ӡ��Ķ�������֮�⣬�������ԣ�������ɫ���������꣩������ԭ�������ˡ�����
	
	˵��ΪɶҪ�á����ӡ��Ķ������ԡ�������������Shadow Map�ľ������⡣����
	�е�ʱ�򱳹������Ȧ�������⡣�������ѿ�����������Ҫ��Shader�ѱ�����Ū��������
	���������shader֪������泯�����أ�������NormalArray���Ҿ�����һ�����ӵĶ������ԡ�����
	0.0f��ʾǰ��(z+)��1.0f��ʾ����(z-)��2.0f��ʾ����(x+)��3.0f��ʾ����(x-)��4.0f��ʾ����(y+)��5.0f��ʾ����(y-)

		��û�п���������Щֵ������ȫ����

			����
				�㣡
					�ͣ�
						�ģ�������������

	�ӵ���GLSL��֧��������Ϊ�������ԡ�����ֻ���ø����ʹ�����(�s�F����)�s��ߩ���
	Ȼ��Ϊ�˽���������ľ������⣬����shader��д�˸���������ȡ��������
	��˵�ˡ�����

	�ȵ��һ�û��ǩ���ء�����
	--qiaozhanrong

	====================================================
	���԰壺

	1¥. qiaozhanrong: �Լ�����ɳ����
	2¥. Null: ���������Դ����д��ôһ���������ɣ�23333333333
	3¥. qiaozhanrong: ���İ�233333333333

	4¥. [����������]: [������ظ�����]

	[�ظ�]
	====================================================
	*/

	int Vertexes, Texcoordc, Colorc, Attribc;
	float* VertexArray = nullptr;
	float* VA = nullptr;
	float TexCoords[3], Colors[4], Attribs;
	//unsigned int Buffers[3];
	bool AdvancedRender;
	int ShadowRes = 4096;
	int MaxShadowDist = 4;
	int shadowdist;
	float sunlightXrot, sunlightYrot;
	vector<Shader> shaders;
	int ActiveShader;
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

		//�ϴβ�֪��ԭ��Flush���г��������˼QAQ
		//OpenGL�и�����glFlush()�������������GL�����() ��_��

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

		//������ǲ��Ǻ�װ��2333 --qiaozhanrong
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
		shaders.reserve(4);
		shaders.push_back(Shader("Shaders/Main.vsh", "Shaders/Main.fsh", true));
		shaders.push_back(Shader("Shaders/Main.vsh", "Shaders/Main.fsh", true, defines));
		shaders.push_back(Shader("Shaders/Shadow.vsh", "Shaders/Shadow.fsh", false));
		shaders.push_back(Shader("Shaders/Depth.vsh", "Shaders/Depth.fsh", false, defines));

		glGenTextures(1, &DepthTexture);
		glBindTexture(GL_TEXTURE_2D, DepthTexture);
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
			shaders[i].bind();
			if (i == 0) shaders[i].setUniform("Tex", 0);
			else shaders[i].setUniform("Tex3D", 0);
			shaders[i].setUniform("DepthTex", 1);
			shaders[i].setUniform("SkyColor", skycolorR, skycolorG, skycolorB, 1.0f);
		}
		Shader::unbind();
	}

	void destroyShaders() {
		for (size_t i = 0; i != shaders.size(); i++)
			shaders[i].release();
		shaders.clear();
		glDeleteTextures(1, &DepthTexture);
		glDeleteFramebuffersEXT(1, &ShadowFBO);
	}

	void EnableShaders() {
		shadowdist = min(MaxShadowDist, viewdistance);

		//Enable shader
		Shader& shader = shaders[MergeFace ? MergeFaceShader : MainShader];
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
		shader.setUniform("renderdist", viewdistance * 16.0f);
		shader.setUniform("Depth_proj", frus.getProjMatrix());
		shader.setUniform("Depth_modl", frus.getModlMatrix());
		
		//Enable arrays for additional vertex attributes
		glEnableVertexAttribArrayARB(ShaderAttribLoc);
	}

	void DisableShaders() {
		//Disable shader
		Shader::unbind();

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
		Shader::unbind();
		glViewport(0, 0, windowwidth, windowheight);
	}

}
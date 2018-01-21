#include "Renderer.h"

#include <vector>
#include <cmath>
#include "Shader.h"

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
	2楼. Null: 这就是你在源码里写这么一长串的理由？23333333333
	3楼. qiaozhanrong: 无聊啊233333333333

	4楼. [请输入姓名]: [请输入回复内容]

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
	int MaxShadowDist = 4;
	int shadowdist;
	float sunlightXrot, sunlightYrot;
	vector<Shader> shaders;
	int ActiveShader;
	int index = 0, size = 0;
	unsigned int ShadowFBO, DepthTexture, SceneFBO, SceneColorTexture, SceneDepthTexture, SSRColorTexture, SSRDepthTexture;
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
		if ((Vertexes + 1) * (Texcoordc + Colorc + 3) > ArraySize) return;
		if (Attribc != 0) VertexArray[index++] = Attribs;
		if (Texcoordc != 0) memcpy(VertexArray + index, TexCoords, Texcoordc * sizeof(float));
		index += Texcoordc;
		if (Colorc != 0) memcpy(VertexArray + index, Colors, Colorc * sizeof(float));
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
				} else {
					glTexCoordPointer(tc, GL_FLOAT, cnt * sizeof(float), (float*)0);
					glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)(tc * sizeof(float)));
				}
			} else {
				if (cc != 0) {
					glColorPointer(cc, GL_FLOAT, cnt * sizeof(float), (float*)0);
					glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)(cc * sizeof(float)));
				} else glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)0);
			}
		} else {
			cnt += ac;
			glVertexAttribPointerARB(ShaderAttribLoc, ac, GL_FLOAT, GL_FALSE, cnt * sizeof(float), (float*)0);
			glTexCoordPointer(tc, GL_FLOAT, cnt * sizeof(float), (float*)(ac * sizeof(float)));
			glColorPointer(cc, GL_FLOAT, cnt * sizeof(float), (float*)((ac + tc) * sizeof(float)));
			glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)((ac + tc + cc) * sizeof(float)));
		}

		//这个框是不是很装逼2333 --qiaozhanrong
		//====================================================================================================//
		/**/                                                                                                /**/
		/**/                                                                                                /**/
		/**/                                glDrawArrays(GL_QUADS, 0, vtxs);                                /**/
		/**/                                                                                                /**/
		/**/                                                                                                /**/
		//====================================================================================================//
	}

	void initShaders() {
		ShaderAttribLoc = 1;
		std::set<string> defines;
		defines.insert("MergeFace");

		sunlightXrot = 30.0f;
		sunlightYrot = 60.0f;
		shadowdist = min(MaxShadowDist, viewdistance);
		shaders.reserve(5);
		shaders.push_back(Shader("Shaders/Main.vsh", "Shaders/Main.fsh", true));
		shaders.push_back(Shader("Shaders/Main.vsh", "Shaders/Main.fsh", true, defines));
		shaders.push_back(Shader("Shaders/Shadow.vsh", "Shaders/Shadow.fsh", false));
		shaders.push_back(Shader("Shaders/Depth.vsh", "Shaders/Depth.fsh", false, defines));
		shaders.push_back(Shader("Shaders/ShowDepth.vsh", "Shaders/ShowDepth.fsh", false, defines));

		glGenTextures(1, &DepthTexture);
		glBindTexture(GL_TEXTURE_2D, DepthTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE_ARB, GL_INTENSITY_EXT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16_ARB, ShadowRes, ShadowRes, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

		glActiveTextureARB(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, DepthTexture);
		glActiveTextureARB(GL_TEXTURE0);

		glGenFramebuffersEXT(1, &ShadowFBO);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, ShadowFBO);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, DepthTexture, 0);
		if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
			DebugError("Frame buffer creation error!");
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

		for (int i = 0; i < 2; i++) {
			shaders[i].bind();
			if (i == 0) shaders[i].setUniform("Texture", 0);
			else shaders[i].setUniform("Texture3D", 0);
			shaders[i].setUniform("DepthTexture", 1);
			shaders[i].setUniform("BackgroundColor", skycolorR, skycolorG, skycolorB, 1.0f);
			shaders[i].setUniform("ShadowMapResolution", float(ShadowRes));
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

	FrustumTest getLightFrustum() {
		FrustumTest res;
		res.LoadIdentity();
		res.SetOrtho(-1, 1, -1, 1, -1, 1);
		res.MultRotate(sunlightXrot, 1.0f, 0.0f, 0.0f);
		res.MultRotate(sunlightYrot, 0.0f, 1.0f, 0.0f);
		return res;
	}

	FrustumTest getShadowMapFrustum(double xpos, double ypos, double zpos, double heading, double pitch, const FrustumTest& frus) {
		FrustumTest lightSpace = getLightFrustum();
		std::vector<Vec3f> vertexes;
		float halfHeight = std::tan(frus.getFOV() / 2.0f);
		float halfWidth = halfHeight * frus.getAspect();
		float pnear = frus.getNear(), pfar = Renderer::shadowdist * 16.0f;
		float nh = halfHeight * pnear, nw = halfWidth * pnear;
		float fh = halfHeight * pfar, fw = halfWidth * pfar;
		vertexes.push_back(Vec3f(-nw, -nh, -pnear));
		vertexes.push_back(Vec3f(nw, -nh, -pnear));
		vertexes.push_back(Vec3f(nw, nh, -pnear));
		vertexes.push_back(Vec3f(-nw, nh, -pnear));
		vertexes.push_back(Vec3f(-fw, -fh, -pfar));
		vertexes.push_back(Vec3f(fw, -fh, -pfar));
		vertexes.push_back(Vec3f(fw, fh, -pfar));
		vertexes.push_back(Vec3f(-fw, fh, -pfar));
		Mat4f frustumRotate = Mat4f::rotation(pitch, Vec3f(1, 0, 0)) * Mat4f::rotation(heading, Vec3f(0, 1, 0));
		Mat4f toLightSpace = frustumRotate * Mat4f(lightSpace.getProjMatrix()) * Mat4f(lightSpace.getModlMatrix());
		for (size_t i = 0; i < vertexes.size(); i++) vertexes[i] = toLightSpace.transformVec3(vertexes[i]);
		Vec3f nearCenter = toLightSpace.transformVec3(Vec3f(0, 0, -pnear));
		Vec3f farCenter = toLightSpace.transformVec3(Vec3f(0, 0, -pfar));

		FrustumTest res = lightSpace;

		// Minimal Bounding Box
		/*
		float xmin = vertexes[0].x, xmax = vertexes[0].x, ymin = vertexes[0].y, ymax = vertexes[0].y, zmin = vertexes[0].z, zmax = vertexes[0].z;

		for (size_t i = 0; i < vertexes.size(); i++) {
			xmin = min(xmin, vertexes[i].x);
			xmax = max(xmax, vertexes[i].x);
			ymin = min(ymin, vertexes[i].y);
			ymax = max(ymax, vertexes[i].y);
			zmin = min(zmin, vertexes[i].z);
			zmax = max(zmax, vertexes[i].z);
		}

		float scale = 16.0f * sqrt(3.0f);
		float length = shadowdist * scale;
		res.SetOrtho(xmin, xmax, ymin, ymax, -1000.0, 1000.0);
		*/

		// Original
		float scale = 16.0f * sqrt(3.0f);
		float length = shadowdist * scale;
		res.SetOrtho(-length, length, -length, length, -1000.0, 1000.0);

		return res;
	}

	void EnableShaders(double xpos, double ypos, double zpos, double heading, double pitch, const FrustumTest& playerFrustum) {
		shadowdist = min(MaxShadowDist, viewdistance);

		//Enable shader
		Shader& shader = shaders[MergeFace ? MergeFaceShader : MainShader];
		bindShader(MergeFace ? MergeFaceShader : MainShader);

		//Calc matrix
		FrustumTest frus = getShadowMapFrustum(xpos, ypos, zpos, heading, pitch, playerFrustum);

		//Set uniform
		shader.setUniform("RenderDistance", viewdistance * 16.0f);
		shader.setUniform("ShadowMapProjection", frus.getProjMatrix());
		shader.setUniform("ShadowMapModelView", frus.getModlMatrix());

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
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDrawBuffer(GL_BACK); glReadBuffer(GL_BACK);
		Shader::unbind();
		glViewport(0, 0, windowwidth, windowheight);
	}

}
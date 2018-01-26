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

	int Vertexes, Texcoordc, Colorc, Normalc, Attribc;
	float* VertexArray = nullptr;
	float* VA = nullptr;
	float TexCoords[3], Colors[4], Normals[3], Attribs;
	//unsigned int Buffers[3];
	bool AdvancedRender;
	int ShadowRes = 4096;
	int MaxShadowDist = 4;
	int shadowdist;
	float sunlightXrot, sunlightYrot;
	vector<Shader> shaders;
	int ActiveShader;
	int index = 0, size = 0;
	unsigned int ShaderAttribLoc = 0;

	const int gBufferCount = 6;
	int gWidth, gHeight, gSize;
	FrameBuffer shadow, gBuffers;

	int log2Ceil(int x) {
		if (x <= 1)return 0;
		x--;
		int res = 1;
		while (x != 1)res++, x >>= 1;
		return res;
	}

	void Init(int tc, int cc, int nc, int ac) {
		Texcoordc = tc; Colorc = cc; Normalc = nc; Attribc = ac;
		if (VertexArray == nullptr) VertexArray = new float[ArraySize];
		index = 0;
		VA = VertexArray;
		Vertexes = 0;
		size = (tc + cc + nc + ac + 3) * 4;
	}

	void resizeGBuffers(int w, int h) {
		gWidth = w, gHeight = h;
		gSize = 1 << log2Ceil(max(gWidth, gHeight));
		gBuffers.create(gSize, gBufferCount, false);
	}

	void Vertex3f(float x, float y, float z) {
		if ((Vertexes + 1) * (Attribc + Texcoordc + Colorc + Normalc + 3) > ArraySize) return;
		if (Attribc != 0) VertexArray[index++] = Attribs;
		if (Texcoordc != 0) memcpy(VertexArray + index, TexCoords, Texcoordc * sizeof(float));
		index += Texcoordc;
		if (Colorc != 0) memcpy(VertexArray + index, Colors, Colorc * sizeof(float));
		index += Colorc;
		if (Normalc != 0) memcpy(VertexArray + index, Normals, Normalc * sizeof(float));
		index += Normalc;
		VertexArray[index++] = x;
		VertexArray[index++] = y;
		VertexArray[index++] = z;
		Vertexes++;
	}

	void TexCoord2f(float x, float y) { TexCoords[0] = x; TexCoords[1] = y; }
	void TexCoord3f(float x, float y, float z) { TexCoords[0] = x; TexCoords[1] = y; TexCoords[2] = z; }
	void Color3f(float r, float g, float b) { Colors[0] = r; Colors[1] = g; Colors[2] = b; }
	void Color4f(float r, float g, float b, float a) { Colors[0] = r; Colors[1] = g; Colors[2] = b; Colors[3] = a; }
	void Normal3f(float x, float y, float z) { Normals[0] = x; Normals[1] = y; Normals[2] = z; }
	void Attrib1f(float a) { Attribs = a; }

	void Flush(VBOID& buffer, vtxCount& vtxs) {

		//上次才知道原来Flush还有冲厕所的意思QAQ
		//OpenGL有个函数glFlush()，翻译过来就是GL冲厕所() ←_←

		vtxs = Vertexes;
		if (Vertexes != 0) {
			if (buffer == 0) glGenBuffersARB(1, &buffer);
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
			glBufferDataARB(GL_ARRAY_BUFFER_ARB,
			                Vertexes * ((Texcoordc + Colorc + Normalc + Attribc + 3) * sizeof(float)),
			                VertexArray, GL_STATIC_DRAW_ARB);
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		}
	}

	void renderbuffer(VBOID buffer, vtxCount vtxs, int tc, int cc, int nc, int ac) {

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
		int cnt = tc + cc + 3;
		if (!AdvancedRender || nc == 0 || ac == 0) {
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
			cnt += nc + ac;
			glVertexAttribPointerARB(ShaderAttribLoc, ac, GL_FLOAT, GL_FALSE, cnt * sizeof(float), (float*)0);
			glTexCoordPointer(tc, GL_FLOAT, cnt * sizeof(float), (float*)(ac * sizeof(float)));
			glColorPointer(cc, GL_FLOAT, cnt * sizeof(float), (float*)((ac + tc) * sizeof(float)));
			glNormalPointer(GL_FLOAT, cnt * sizeof(float), (float*)((ac + tc + cc) * sizeof(float)));
			glVertexPointer(3, GL_FLOAT, cnt * sizeof(float), (float*)((ac + tc + cc + nc) * sizeof(float)));
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
		shaders.reserve(6);
		shaders.push_back(Shader("Shaders/Main.vsh", "Shaders/Main.fsh", true));
		shaders.push_back(Shader("Shaders/Main.vsh", "Shaders/Main.fsh", true, defines));
		shaders.push_back(Shader("Shaders/Final.vsh", "Shaders/Final.fsh", false));
		shaders.push_back(Shader("Shaders/Shadow.vsh", "Shaders/Shadow.fsh", false));
		shaders.push_back(Shader("Shaders/Depth.vsh", "Shaders/Depth.fsh", false, defines));
		shaders.push_back(Shader("Shaders/ShowDepth.vsh", "Shaders/ShowDepth.fsh", false));

		// framebuffers

		shadow.create(ShadowRes, 0, true);
		resizeGBuffers(windowwidth, windowheight);

		// load shaders

		for (int i = 0; i < 2; i++) {
			shaders[i].bind();
			if (i == 0) shaders[i].setUniform("Texture", 0);
			else shaders[i].setUniform("Texture3D", 0);
			shaders[i].setUniform("DepthTexture", 1);
			shaders[i].setUniform("BackgroundColor", skycolorR, skycolorG, skycolorB, 1.0f);
			shaders[i].setUniform("ShadowMapResolution", float(ShadowRes));
		}

		shaders[FinalShader].bind();
		shaders[FinalShader].setUniform("Texture0", 0);
		shaders[FinalShader].setUniform("Texture1", 1);
		shaders[FinalShader].setUniform("Texture2", 2);
		shaders[FinalShader].setUniform("Texture3", 3);
		shaders[FinalShader].setUniform("Texture4", 4);
		shaders[FinalShader].setUniform("Texture5", 5);
		shaders[FinalShader].setUniform("BackgroundColor", skycolorR, skycolorG, skycolorB, 1.0f);

		Shader::unbind();
	}

	void destroyShaders() {
		for (size_t i = 0; i != shaders.size(); i++)
			shaders[i].release();
		shaders.clear();
		shadow.destroy();
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

	void ClearBuffer() {
		gBuffers.bindTarget();
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		gBuffers.bindTargetBuffer(0);
		glClearColor(skycolorR, skycolorG, skycolorB, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		gBuffers.unbindTarget();
	}

	void EnableDefferedRendering(double xpos, double ypos, double zpos, double heading, double pitch, const FrustumTest& playerFrustum) {
		gBuffers.bindTarget(gWidth, gHeight);
		
		shadowdist = min(MaxShadowDist, viewdistance);
		shadow.bindDepthTexture(1);

		//Enable shader
		Shader& shader = shaders[MergeFace ? MergeFaceShader : MainShader];
		bindShader(MergeFace ? MergeFaceShader : MainShader);

		//Calc matrix
		FrustumTest frus = getShadowMapFrustum(xpos, ypos, zpos, heading, pitch, playerFrustum);

		//Set uniform
		shader.setUniform("RenderDistance", viewdistance * 16.0f);
		shader.setUniform("ShadowMapProjection", frus.getProjMatrix());
		shader.setUniform("ShadowMapModelView", frus.getModlMatrix());
		Mat4f trans = Mat4f::rotation(-sunlightXrot, Vec3f(1, 0, 0)) * Mat4f::rotation(-sunlightYrot, Vec3f(0, 1, 0));
		Vec3f lightdir = trans.transformVec3(Vec3f(0, 0, -1));
		lightdir.normalize();
		shader.setUniform("SunlightDirection", lightdir.x, lightdir.y, lightdir.z);

		//Enable arrays for additional vertex attributes
		glEnable(GL_NORMAL_ARRAY);
		glEnableVertexAttribArrayARB(ShaderAttribLoc);
	}

	void DisableDefferedRendering() {
		gBuffers.unbindTarget();

		//Disable shader
		Shader::unbind();

		//Disable arrays for additional vertex attributes
		glDisable(GL_NORMAL_ARRAY);
		glDisableVertexAttribArrayARB(ShaderAttribLoc);
	}

	void StartShadowPass() {
		shadow.bindTarget();
		bindShader(ShadowShader);
	}

	void EndShadowPass() {
		shadow.unbindTarget();
		Shader::unbind();
	}

	void StartFinalPass(const FrustumTest& viewFrustum) {
		gBuffers.bindColorTextures(0);
		bindShader(FinalShader);
		shaders[FinalShader].setUniform("ScreenWidth", float(gWidth));
		shaders[FinalShader].setUniform("ScreenHeight", float(gHeight));
		shaders[FinalShader].setUniform("BufferSize", float(gSize));
		shaders[FinalShader].setUniform("ProjectionMatrix", viewFrustum.getProjMatrix());
		shaders[FinalShader].setUniform("NormalMatrix", viewFrustum.getModlMatrix());
		shaders[FinalShader].setUniform("RenderDistance", viewdistance * 16.0f);
	}

	void EndFinalPass() {
		Shader::unbind();
	}

	void DrawFullscreen() {
		glDepthFunc(GL_ALWAYS);
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f); glVertex2i(0, windowheight);
		glTexCoord2f(1.0f, 0.0f); glVertex2i(windowwidth, windowheight);
		glTexCoord2f(1.0f, 1.0f); glVertex2i(windowwidth, 0);
		glTexCoord2f(0.0f, 1.0f); glVertex2i(0, 0);
		glEnd();
	}
}
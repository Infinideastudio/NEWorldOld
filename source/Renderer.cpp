#include "Renderer.h"

#include <vector>
#include <cmath>
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

	int Vertexes, Texcoordc, Colorc, Normalc, Attribc;
	float* VertexArray = nullptr;
	float* VA = nullptr;
	float TexCoords[3], Colors[4], Normals[3], Attribs;
	//unsigned int Buffers[3];
	bool AdvancedRender;
	int ShadowRes = 4096;
	int MaxShadowDist = 6;
	int shadowdist;
	float sunlightXrot, sunlightYrot;
	vector<Shader> shaders;
	int ActiveShader;
	int index = 0, size = 0;
	unsigned int ShaderAttribLoc = 0;

	const int gBufferCount = 4;
	int gWidth, gHeight, gSize;
	FrameBuffer shadow, gBuffers;
	bool VolumetricClouds = false;

	int log2Ceil(int x) {
		if (x <= 1)return 0;
		x--;
		int res = 1;
		while (x != 1)res++, x >>= 1;
		return res;
	}

	void Init(int tc, int cc, int nc, int ac) {
		Texcoordc = tc;
		Colorc = cc;
		Normalc = nc;
		Attribc = ac;
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

		//�ϴβ�֪��ԭ��Flush���г��������˼QAQ
		//OpenGL�и�����glFlush()�������������GL�����() ��_��

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

		//������ǲ��Ǻ�װ��2333 --qiaozhanrong
		//====================================================================================================//
		/**/                                                                                                /**/
		/**/                                                                                                /**/
		/**/                                glDrawArrays(GL_QUADS, 0, vtxs);                                /**/
		/**/                                                                                                /**/
		/**/                                                                                                /**/
		//====================================================================================================//
	}

	GLuint getNoiseTexture() {
		static GLuint noiseTex = 0;
		if (noiseTex == 0) {
			unique_ptr<ubyte[]> a(new ubyte[256 * 256 * 4]);
			for (int i = 0; i < 256 * 256; i++) a[i * 4] = a[i * 4 + 1] = int(rnd() * 256);

			const int OffsetX = 37, OffsetY = 17;
			for (int x = 0; x < 256; x++) for (int y = 0; y < 256; y++) {
					int x1 = (x + OffsetX) % 256, y1 = (y + OffsetY) % 256;
					a[(y * 256 + x) * 4 + 2] = a[(y1 * 256 + x1) * 4];
					a[(y * 256 + x) * 4 + 3] = a[(y1 * 256 + x1) * 4 + 1];
				}

			glGenTextures(1, &noiseTex);
			glBindTexture(GL_TEXTURE_2D, noiseTex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, a.get());
		}
		return noiseTex;
	}

	void initShaders() {
		ShaderAttribLoc = 1;
		std::set<string> mgf, defines;
		mgf.insert("MergeFace");
		if (VolumetricClouds) defines.insert("VolumetricClouds");

		sunlightXrot = 40.0f;
		sunlightYrot = 60.0f;
		shadowdist = min(MaxShadowDist, viewdistance);
		shaders.reserve(5);
		shaders.push_back(Shader("Shaders/Main.vsh", "Shaders/Main.fsh", true));
		shaders.push_back(Shader("Shaders/Main.vsh", "Shaders/Main.fsh", true, mgf));
		shaders.push_back(Shader("Shaders/Final.vsh", "Shaders/Final.fsh", false, defines));
		shaders.push_back(Shader("Shaders/Shadow.vsh", "Shaders/Shadow.fsh", true));
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
			shaders[i].setUniform("NoiseTexture", 2);
			shaders[i].setUniform("BackgroundColor", skycolorR, skycolorG, skycolorB, 1.0f);
			shaders[i].setUniform("ShadowMapResolution", float(ShadowRes));
		}

		shaders[FinalShader].bind();
		shaders[FinalShader].setUniform("Texture0", 0);
		shaders[FinalShader].setUniform("Texture1", 1);
		shaders[FinalShader].setUniform("Texture2", 2);
		shaders[FinalShader].setUniform("Texture3", 3);
		shaders[FinalShader].setUniform("NoiseTexture", gBufferCount);
		shaders[FinalShader].setUniform("BackgroundColor", skycolorR, skycolorG, skycolorB, 1.0f);

		shaders[ShadowShader].bind();
		shaders[ShadowShader].setUniform("Texture", 0);

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
		float scale = 16.0f;// *sqrt(3.0f);
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

	void EnableDefferedRendering(double xpos, double ypos, double zpos, double heading, double pitch, const FrustumTest& playerFrustum, float gameTime) {
		gBuffers.bindTarget(gWidth, gHeight);

		shadowdist = min(MaxShadowDist, viewdistance);
		shadow.bindDepthTexture(1);
		glActiveTextureARB(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, getNoiseTexture());
		glActiveTextureARB(GL_TEXTURE0);

		//Enable shader
		Shader& shader = shaders[MergeFace ? MergeFaceShader : MainShader];
		bindShader(MergeFace ? MergeFaceShader : MainShader);

		//Calc matrix
		FrustumTest frus = getShadowMapFrustum(xpos, ypos, zpos, heading, pitch, playerFrustum);

		//Set uniform
		shader.setUniform("RenderDistance", viewdistance * 16.0f);
		shader.setUniform("ShadowDistance", shadowdist * 16.0f);
		shader.setUniform("ShadowMapProjection", frus.getProjMatrix());
		shader.setUniform("ShadowMapModelView", frus.getModlMatrix());
		Mat4f trans = Mat4f::rotation(-sunlightXrot, Vec3f(1, 0, 0)) * Mat4f::rotation(-sunlightYrot, Vec3f(0, 1, 0));
		Vec3f lightdir = trans.transformVec3(Vec3f(0, 0, -1));
		lightdir.normalize();
		shader.setUniform("SunlightDirection", lightdir.x, lightdir.y, lightdir.z);
		shader.setUniform("GameTime", gameTime);
		shader.setUniform3i("PlayerPositionInt", int(floor(xpos)), int(floor(ypos)), int(floor(zpos)));
		shader.setUniform("PlayerPositionFrac", xpos - floor(xpos), ypos - floor(ypos), zpos - floor(zpos));

		//Enable arrays for additional vertex attributes
		glEnableClientState(GL_NORMAL_ARRAY);
		glEnableVertexAttribArrayARB(ShaderAttribLoc);
		glDisable(GL_FOG);
	}

	void DisableDefferedRendering() {
		gBuffers.unbindTarget();

		//Disable shader
		Shader::unbind();

		//Disable arrays for additional vertex attributes
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableVertexAttribArrayARB(ShaderAttribLoc);
		glEnable(GL_FOG);
	}

	void StartShadowPass(float gameTime) {
		shadow.bindTarget();
		bindShader(ShadowShader);
		shaders[ShadowShader].setUniform("GameTime", gameTime);
		glEnableVertexAttribArrayARB(ShaderAttribLoc);
	}

	void EndShadowPass() {
		shadow.unbindTarget();
		Shader::unbind();
		glDisableVertexAttribArrayARB(ShaderAttribLoc);
	}

	void StartFinalPass(double xpos, double ypos, double zpos, const FrustumTest& viewFrustum) {
		gBuffers.bindColorTextures(0);
		glActiveTextureARB(GL_TEXTURE0 + gBufferCount);
		glBindTexture(GL_TEXTURE_2D, getNoiseTexture());
		glActiveTextureARB(GL_TEXTURE0);

		bindShader(FinalShader);
		shaders[FinalShader].setUniform("ScreenWidth", float(gWidth));
		shaders[FinalShader].setUniform("ScreenHeight", float(gHeight));
		shaders[FinalShader].setUniform("BufferSize", float(gSize));
		shaders[FinalShader].setUniform("ProjectionMatrix", viewFrustum.getProjMatrix());
		shaders[FinalShader].setUniform("ModelViewMatrix", viewFrustum.getModlMatrix());
		shaders[FinalShader].setUniform("ProjectionInverse", Mat4f(viewFrustum.getProjMatrix()).inverse().data);
		shaders[FinalShader].setUniform("ModelViewInverse", Mat4f(viewFrustum.getModlMatrix()).inverse().data);
		//shaders[FinalShader].setUniform("FOVx", viewFrustum.getFOV() * viewFrustum.getAspect());
		//shaders[FinalShader].setUniform("FOVy", viewFrustum.getFOV());
		shaders[FinalShader].setUniform("RenderDistance", viewdistance * 16.0f);

		Mat4f trans = Mat4f::rotation(-sunlightXrot, Vec3f(1, 0, 0)) * Mat4f::rotation(-sunlightYrot, Vec3f(0, 1, 0));
		Vec3f lightdir = trans.transformVec3(Vec3f(0, 0, -1));
		lightdir.normalize();
		shaders[FinalShader].setUniform("SunlightDirection", lightdir.x, lightdir.y, lightdir.z);
		shaders[FinalShader].setUniform3i("PlayerPositionInt", int(floor(xpos)), int(floor(ypos)), int(floor(zpos)));
		shaders[FinalShader].setUniform("PlayerPositionFrac", xpos - floor(xpos), ypos - floor(ypos), zpos - floor(zpos));
	}

	void EndFinalPass() {
		Shader::unbind();
	}

	void DrawFullscreen() {
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f);
		glVertex2i(0, windowheight);
		glTexCoord2f(1.0f, 0.0f);
		glVertex2i(windowwidth, windowheight);
		glTexCoord2f(1.0f, 1.0f);
		glVertex2i(windowwidth, 0);
		glTexCoord2f(0.0f, 1.0f);
		glVertex2i(0, 0);
		glEnd();
	}
}
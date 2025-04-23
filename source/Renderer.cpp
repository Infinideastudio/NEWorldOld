#include "Renderer.h"

#include <vector>
#include <cmath>
#include "Shader.h"

namespace Renderer {

	int Vertexes, Texcoordc, Colorc, Normalc, Attribc;
	float* VertexArray = nullptr;
	float* VA = nullptr;
	float TexCoords[3], Colors[4], Normals[3], Attribs;
	//unsigned int Buffers[3];
	bool AdvancedRender;
	int ShadowRes = 4096;
	int MaxShadowDist = 6;
	float sunlightXrot, sunlightYrot;
	vector<Shader> shaders;
	int ActiveShader;
	int index = 0, size = 0;
	unsigned int ShaderAttribLoc = 0;

	const int gBufferCount = 3;
	int gWidth, gHeight, gSize;
	FrameBuffer shadow, gBuffers, dBuffer;
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

	void resizeGDBuffers(int w, int h) {
		gWidth = w, gHeight = h;
		gSize = 1 << log2Ceil(max(gWidth, gHeight));
		gBuffers.create(gSize, gBufferCount, true, false);
		dBuffer.create(gSize, 0, true, false);
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

	void Flush(VBOID& buffer, GLuint& vtxs) {

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

	void renderbuffer(VBOID buffer, GLuint vtxs, int tc, int cc, int nc, int ac) {

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
			unique_ptr<uint8_t[]> a(new uint8_t[256 * 256 * 4]);
			for (int i = 0; i < 256 * 256; i++) a[i * 4] = a[i * 4 + 1] = static_cast<uint8_t>(rnd() * 256);

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
		std::set<string> defines;
		if (MergeFace) defines.insert("MergeFace");
		if (VolumetricClouds) defines.insert("VolumetricClouds");

		sunlightXrot = 30.0f;
		sunlightYrot = 60.0f;
		shaders.clear();
		shaders.push_back(Shader("Shaders/Base.vsh", "Shaders/Base.fsh", true, defines));
		shaders.push_back(Shader("Shaders/Translucent.vsh", "Shaders/Translucent.fsh", true, defines));
		shaders.push_back(Shader("Shaders/Final.vsh", "Shaders/Final.fsh", false, defines));
		shaders.push_back(Shader("Shaders/Shadow.vsh", "Shaders/Shadow.fsh", true, defines));
		shaders.push_back(Shader("Shaders/ShowDepth.vsh", "Shaders/ShowDepth.fsh", false, defines));

		// Create framebuffers
		shadow.create(ShadowRes, 0, true, true);
		resizeGDBuffers(windowwidth, windowheight);

		// Set constant uniforms
		shaders[BaseShader].bind();
		shaders[BaseShader].setUniform(MergeFace ? "Texture3D" : "Texture", 0);

		shaders[TranslucentShader].bind();
		shaders[TranslucentShader].setUniform(MergeFace ? "Texture3D" : "Texture", 0);

		float fisheyeFactor = MergeFace ? 0.0f : 0.85f;
		shaders[FinalShader].bind();
		shaders[FinalShader].setUniform("Texture0", 0);
		shaders[FinalShader].setUniform("Texture1", 1);
		shaders[FinalShader].setUniform("Texture2", 2);
		shaders[FinalShader].setUniform("DepthTexture", gBufferCount + 0);
		shaders[FinalShader].setUniform("ShadowTexture", gBufferCount + 1);
		shaders[FinalShader].setUniform("NoiseTexture", gBufferCount + 2);
		shaders[FinalShader].setUniform("ShadowMapResolution", float(ShadowRes));
		shaders[FinalShader].setUniform("ShadowFisheyeFactor", fisheyeFactor);

		shaders[ShadowShader].bind();
		shaders[ShadowShader].setUniform("Texture", 0);
		shaders[ShadowShader].setUniform("ShadowFisheyeFactor", fisheyeFactor);

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

	FrustumTest getShadowMapFrustum(double heading, double pitch, int shadowdist, const FrustumTest& frus) {
		FrustumTest lightSpace = getLightFrustum();
		std::vector<Vec3f> vertexes;
		float halfHeight = std::tan(frus.getFOV() / 2.0f);
		float halfWidth = halfHeight * frus.getAspect();
		float pnear = frus.getNear(), pfar = shadowdist * 16.0f;
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
		Mat4f frustumRotate = Mat4f::rotation(static_cast<float>(pitch), Vec3f(1, 0, 0)) * Mat4f::rotation(static_cast<float>(heading), Vec3f(0, 1, 0));
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

	void ClearSGDBuffers() {
		shadow.bindTarget();
		glClearDepth(1.0f);
		glClear(GL_DEPTH_BUFFER_BIT);
		shadow.unbindTarget();

		gBuffers.bindTarget();
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClearDepth(1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		gBuffers.unbindTarget();

		dBuffer.bindTarget();
		glClearDepth(1.0f);
		glClear(GL_DEPTH_BUFFER_BIT);
		dBuffer.unbindTarget();
	}

	void StartShadowPass() {
		// Bind output target buffers
		shadow.bindTarget();

		// Enable shader
		bindShader(ShadowShader);

		// Enable arrays for additional vertex attributes
		glEnableVertexAttribArrayARB(ShaderAttribLoc);

		// Disable unwanted defaults
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glDisable(GL_FOG);
	}

	void EndShadowPass() {
		// Unbind output target buffers
		shadow.unbindTarget();

		// Disable shader
		Shader::unbind();

		// Disable arrays for additional vertex attributes
		glDisableVertexAttribArrayARB(ShaderAttribLoc);

		// Enable defaults
		glEnable(GL_ALPHA_TEST);
		glEnable(GL_BLEND);
		glEnable(GL_CULL_FACE);
		glEnable(GL_FOG);
	}

	void StartBasePass(float gameTime) {
		// Bind output target buffers
		gBuffers.bindTarget(gWidth, gHeight);

		// Enable shader
		bindShader(BaseShader);
		Shader& shader = shaders[BaseShader];
		shader.setUniform("GameTime", gameTime);

		// Enable arrays for additional vertex attributes
		glEnableClientState(GL_NORMAL_ARRAY);
		glEnableVertexAttribArrayARB(ShaderAttribLoc);

		// Disable unwanted defaults
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_FOG);
	}

	void EndBasePass() {
		// Unbind output target buffers
		gBuffers.unbindTarget();

		// Disable shader
		Shader::unbind();

		// Disable arrays for additional vertex attributes
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableVertexAttribArrayARB(ShaderAttribLoc);

		// Enable defaults
		glEnable(GL_ALPHA_TEST);
		glEnable(GL_FOG);
	}

	void StartTranslucentPass(float gameTime) {
		// Copy the depth component of the G-buffer to the D-buffer, bind output target buffers
		// gBuffers.copyDepthTexture(dBuffer);
		gBuffers.bindTarget(gWidth, gHeight);

		// Enable shader
		bindShader(TranslucentShader);
		Shader& shader = shaders[TranslucentShader];
		shader.setUniform("GameTime", gameTime);

		// Enable arrays for additional vertex attributes
		glEnableClientState(GL_NORMAL_ARRAY);
		glEnableVertexAttribArrayARB(ShaderAttribLoc);

		// Disable unwanted defaults
		glDisable(GL_ALPHA_TEST);
		// glDisable(GL_BLEND);
		glDisable(GL_FOG);
	}

	void EndTranslucentPass() {
		gBuffers.unbindTarget();

		// Disable shader
		Shader::unbind();

		// Disable arrays for additional vertex attributes
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableVertexAttribArrayARB(ShaderAttribLoc);

		// Enable defaults
		glEnable(GL_ALPHA_TEST);
		// glEnable(GL_BLEND);
		glEnable(GL_FOG);
	}

	void StartFinalPass(double xpos, double ypos, double zpos, double heading, double pitch, const FrustumTest& viewFrustum, float gameTime) {
		// Bind textures to pre-defined slots
		gBuffers.bindColorTextures(0);
		gBuffers.bindDepthTexture(gBufferCount + 0);
		shadow.bindDepthTexture(gBufferCount + 1);
		glActiveTextureARB(GL_TEXTURE0 + gBufferCount + 2);
		glBindTexture(GL_TEXTURE_2D, getNoiseTexture());
		glActiveTextureARB(GL_TEXTURE0);

		// Set dynamic uniforms
		Shader& shader = shaders[FinalShader];
		bindShader(FinalShader);
		shader.setUniform("ScreenWidth", float(gWidth));
		shader.setUniform("ScreenHeight", float(gHeight));
		shader.setUniform("BufferSize", float(gSize));
		shader.setUniform("ProjectionMatrix", viewFrustum.getProjMatrix());
		shader.setUniform("ModelViewMatrix", viewFrustum.getModlMatrix());
		shader.setUniform("ProjectionInverse", Mat4f(viewFrustum.getProjMatrix()).inverse().data);
		shader.setUniform("ModelViewInverse", Mat4f(viewFrustum.getModlMatrix()).inverse().data);
		//shader.setUniform("FOVx", viewFrustum.getFOV() * viewFrustum.getAspect());
		//shader.setUniform("FOVy", viewFrustum.getFOV());

		int shadowdist = min(MaxShadowDist, viewdistance);
		shader.setUniform("RenderDistance", viewdistance * 16.0f);
		shader.setUniform("ShadowDistance", shadowdist * 16.0f);
		FrustumTest frus = getShadowMapFrustum(heading, pitch, shadowdist, viewFrustum);
		shader.setUniform("ShadowMapProjection", frus.getProjMatrix());
		shader.setUniform("ShadowMapModelView", frus.getModlMatrix());

		Mat4f trans = Mat4f::rotation(-sunlightXrot, Vec3f(1, 0, 0)) * Mat4f::rotation(-sunlightYrot, Vec3f(0, 1, 0));
		Vec3f lightdir = trans.transformVec3(Vec3f(0, 0, -1));
		lightdir.normalize();
		shader.setUniform("SunlightDirection", lightdir.x, lightdir.y, lightdir.z);
		shader.setUniform("GameTime", gameTime);
		shader.setUniformI("PlayerPositionInt", int(floor(xpos)), int(floor(ypos)), int(floor(zpos)));
		shader.setUniform("PlayerPositionFrac", float(xpos - floor(xpos)), float(ypos - floor(ypos)), float(zpos - floor(zpos)));
	}

	void EndFinalPass() {
		Shader::unbind();
	}

	void DrawFullscreen() {
		glBegin(GL_QUADS);
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
		glTexCoord2f(0.0f, 0.0f); glVertex2i(0, windowheight);
		glTexCoord2f(1.0f, 0.0f); glVertex2i(windowwidth, windowheight);
		glTexCoord2f(1.0f, 1.0f); glVertex2i(windowwidth, 0);
		glTexCoord2f(0.0f, 1.0f); glVertex2i(0, 0);
		glEnd();
	}
}

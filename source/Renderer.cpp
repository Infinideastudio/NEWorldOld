#include "Renderer.h"

#include <vector>
#include <cmath>
#include "Shader.h"

namespace Renderer {

	int Vertexes, Coordc, Texcoordc, Colorc, Normalc, Attribc;
	float* VertexArray = nullptr;
	float Coords[3], TexCoords[3], Colors[4], Normals[3], Attribs[1];
	//unsigned int Buffers[3];
	bool AdvancedRender;
	int ShadowRes = 4096;
	int MaxShadowDist = 6;
	float sunlightXrot, sunlightYrot;
	vector<Shader> shaders;
	int ActiveShader;
	int index = 0, size = 0;

	const int gBufferCount = 3;
	int gWidth, gHeight, gSize;
	FrameBuffer shadow, gBuffers, dBuffer;
	bool VolumetricClouds = false;

	int log2Ceil(int x) {
		if (x <= 1) return 0;
		x--;
		int res = 1;
		while (x != 1) res++, x >>= 1;
		return res;
	}

	void Begin(int tc, int cc, int nc, int ac) {
		Coordc = 3;
		Texcoordc = tc;
		Colorc = cc;
		Normalc = nc;
		Attribc = ac;
		if (VertexArray == nullptr) VertexArray = new float[ArraySize];
		index = 0;
		Vertexes = 0;
		size = (3 + tc + cc + nc + ac) * 4;
	}

	void resizeGDBuffers(int w, int h) {
		gWidth = w, gHeight = h;
		gSize = 1 << log2Ceil(max(gWidth, gHeight));
		gBuffers.create(gSize, gBufferCount, true, false);
		dBuffer.create(gSize, 0, true, false);
	}

	void Vertex3f(float x, float y, float z) {
		Coords[0] = x; Coords[1] = y; Coords[2] = z;
		if ((Vertexes + 1) * (Coordc + Texcoordc + Colorc + Normalc + Attribc) > ArraySize) return;
		Vertexes++;
		if (Coordc != 0) memcpy(VertexArray + index, Coords, Coordc * sizeof(float));
		index += Coordc;
		if (Texcoordc != 0) memcpy(VertexArray + index, TexCoords, Texcoordc * sizeof(float));
		index += Texcoordc;
		if (Colorc != 0) memcpy(VertexArray + index, Colors, Colorc * sizeof(float));
		index += Colorc;
		if (Normalc != 0) memcpy(VertexArray + index, Normals, Normalc * sizeof(float));
		index += Normalc;
		if (Attribc != 0) memcpy(VertexArray + index, Attribs, Attribc * sizeof(float));
		index += Attribc;
	}
	void TexCoord2f(float x, float y) { TexCoords[0] = x; TexCoords[1] = y; }
	void TexCoord3f(float x, float y, float z) { TexCoords[0] = x; TexCoords[1] = y; TexCoords[2] = z; }
	void Color3f(float r, float g, float b) { Colors[0] = r; Colors[1] = g; Colors[2] = b; }
	void Color4f(float r, float g, float b, float a) { Colors[0] = r; Colors[1] = g; Colors[2] = b; Colors[3] = a; }
	void Normal3f(float x, float y, float z) { Normals[0] = x; Normals[1] = y; Normals[2] = z; }
	void Attrib1f(float a) { Attribs[0] = a; }

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
		std::set<string> defines;
		if (MergeFace) defines.insert("MERGE_FACE");
		if (VolumetricClouds) defines.insert("VOLUMETRIC_CLOUDS");

		sunlightXrot = 30.0f;
		sunlightYrot = 60.0f;
		shaders.clear();
		shaders.push_back(Shader("Shaders/Default.vsh", "Shaders/Default.fsh", true, defines));
		shaders.push_back(Shader("Shaders/Opaque.vsh", "Shaders/Opaque.fsh", true, defines));
		shaders.push_back(Shader("Shaders/Translucent.vsh", "Shaders/Translucent.fsh", true, defines));
		shaders.push_back(Shader("Shaders/Final.vsh", "Shaders/Final.fsh", false, defines));
		shaders.push_back(Shader("Shaders/Shadow.vsh", "Shaders/Shadow.fsh", true, defines));
		shaders.push_back(Shader("Shaders/DebugShadow.vsh", "Shaders/DebugShadow.fsh", false, defines));

		// Create framebuffers
		shadow.create(ShadowRes, 0, true, true);
		resizeGDBuffers(windowwidth, windowheight);

		// Set constant uniforms
		shaders[DefaultShader].bind();
		shaders[DefaultShader].setUniform(MergeFace ? "u_diffuse_3d" : "u_diffuse", 0);

		shaders[OpqaueShader].bind();
		shaders[OpqaueShader].setUniform(MergeFace ? "u_diffuse_3d" : "u_diffuse", 0);

		shaders[TranslucentShader].bind();
		shaders[TranslucentShader].setUniform(MergeFace ? "u_diffuse_3d" : "u_diffuse", 0);

		float fisheyeFactor = MergeFace ? 0.0f : 0.85f;
		shaders[FinalShader].bind();
		shaders[FinalShader].setUniform("u_diffuse_buffer", 0);
		shaders[FinalShader].setUniform("u_normal_buffer", 1);
		shaders[FinalShader].setUniform("u_material_buffer", 2);
		shaders[FinalShader].setUniform("u_depth_buffer", gBufferCount + 0);
		shaders[FinalShader].setUniform("u_shadow_texture", gBufferCount + 1);
		shaders[FinalShader].setUniform("u_noise_texture", gBufferCount + 2);
		shaders[FinalShader].setUniform("u_shadow_texture_resolution", float(ShadowRes));
		shaders[FinalShader].setUniform("u_shadow_fisheye_factor", fisheyeFactor);

		shaders[ShadowShader].bind();
		shaders[ShadowShader].setUniform(MergeFace ? "u_diffuse_3d" : "u_diffuse", 0);
		shaders[ShadowShader].setUniform("u_shadow_fisheye_factor", fisheyeFactor);

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

	void StartShadowPass(const FrustumTest& lightFrustum, float gameTime) {
		assert(AdvancedRender);

		// Bind output target buffers
		shadow.bindTarget();

		// Set dynamic uniforms
		Shader& shader = shaders[ShadowShader];
		bindShader(ShadowShader);
		shader.setUniform("u_proj", lightFrustum.getProjMatrix());
		shader.setUniform("u_modl", lightFrustum.getModlMatrix());
		shader.setUniform("u_game_time", gameTime);

		// Disable unwanted defaults
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
	}

	void EndShadowPass() {
		assert(AdvancedRender);

		// Unbind output target buffers
		shadow.unbindTarget();

		// Disable shader
		Shader::unbind();

		// Enable defaults
		glEnable(GL_BLEND);
		glEnable(GL_CULL_FACE);
	}

	void StartOpqauePass(const FrustumTest& viewFrustum, float gameTime) {
		// Bind output target buffers
		if (AdvancedRender) gBuffers.bindTarget(gWidth, gHeight);

		// Set dynamic uniforms
		Shader& shader = shaders[AdvancedRender ? OpqaueShader : DefaultShader];
		bindShader(AdvancedRender ? OpqaueShader : DefaultShader);
		shader.setUniform("u_proj", viewFrustum.getProjMatrix());
		shader.setUniform("u_modl", viewFrustum.getModlMatrix());
		shader.setUniform("u_game_time", gameTime);

		// Disable unwanted defaults
		glDisable(GL_BLEND);
	}

	void EndOpaquePass() {
		// Unbind output target buffers
		if (AdvancedRender) gBuffers.unbindTarget();

		// Disable shader
		Shader::unbind();

		// Enable defaults
		glEnable(GL_BLEND);
	}

	void StartTranslucentPass(const FrustumTest& viewFrustum, float gameTime) {
		// Copy the depth component of the G-buffer to the D-buffer, bind output target buffers
		if (AdvancedRender) {
			// gBuffers.copyDepthTexture(dBuffer);
			gBuffers.bindTarget(gWidth, gHeight);
		}

		// Set dynamic uniforms
		Shader& shader = shaders[AdvancedRender ? TranslucentShader : DefaultShader];
		bindShader(AdvancedRender ? TranslucentShader : DefaultShader);
		shader.setUniform("u_proj", viewFrustum.getProjMatrix());
		shader.setUniform("u_modl", viewFrustum.getModlMatrix());
		shader.setUniform("u_game_time", gameTime);

		// Disable unwanted defaults
		glDisable(GL_CULL_FACE);
	}

	void EndTranslucentPass() {
		// Unbind output target buffers
		if (AdvancedRender) gBuffers.unbindTarget();

		// Disable shader
		Shader::unbind();

		// Enable defaults
		glEnable(GL_CULL_FACE);
	}

	void StartFinalPass(double xpos, double ypos, double zpos, double heading, double pitch, const FrustumTest& viewFrustum, float gameTime) {
		assert(AdvancedRender);

		// Bind textures to pre-defined slots
		gBuffers.bindColorTextures(0);
		gBuffers.bindDepthTexture(gBufferCount + 0);
		shadow.bindDepthTexture(gBufferCount + 1);
		glActiveTexture(GL_TEXTURE0 + gBufferCount + 2);
		glBindTexture(GL_TEXTURE_2D, getNoiseTexture());
		glActiveTexture(GL_TEXTURE0);

		// Set dynamic uniforms
		int shadowdist = min(MaxShadowDist, viewdistance);
		FrustumTest frus = getShadowMapFrustum(heading, pitch, shadowdist, viewFrustum);
		Vec3f lightdir = (Mat4f::rotation(-sunlightXrot, Vec3f(1, 0, 0)) * Mat4f::rotation(-sunlightYrot, Vec3f(0, 1, 0))).transformVec3(Vec3f(0, 0, -1));
		lightdir.normalize();

		Shader& shader = shaders[FinalShader];
		bindShader(FinalShader);
		shader.setUniform("u_buffer_view_width", float(gWidth));
		shader.setUniform("u_buffer_view_height", float(gHeight));
		shader.setUniform("u_buffer_size", float(gSize));
		shader.setUniform("u_proj", viewFrustum.getProjMatrix());
		shader.setUniform("u_modl", viewFrustum.getModlMatrix());
		shader.setUniform("u_proj_inv", Mat4f(viewFrustum.getProjMatrix()).inverse().data);
		shader.setUniform("u_modl_inv", Mat4f(viewFrustum.getModlMatrix()).inverse().data);
		//shader.setUniform("u_fov_x", viewFrustum.getFOV() * viewFrustum.getAspect());
		//shader.setUniform("u_fov_y", viewFrustum.getFOV());
		shader.setUniform("u_render_distance", viewdistance * 16.0f);
		shader.setUniform("u_shadow_distance", shadowdist * 16.0f);
		shader.setUniform("u_shadow_proj", frus.getProjMatrix());
		shader.setUniform("u_shadow_modl", frus.getModlMatrix());
		shader.setUniform("u_sunlight_dir", lightdir.x, lightdir.y, lightdir.z);
		shader.setUniform("u_game_time", gameTime);
		shader.setUniformI("u_player_coord_int", int(floor(xpos)), int(floor(ypos)), int(floor(zpos)));
		shader.setUniform("u_player_coord_frac", float(xpos - floor(xpos)), float(ypos - floor(ypos)), float(zpos - floor(zpos)));
	}

	void EndFinalPass() {
		assert(AdvancedRender);

		Shader::unbind();
	}

	void DrawFullscreen() {
		Renderer::Begin(2, 0, 0, 0);
		Renderer::TexCoord2f(0.0f, 0.0f); Renderer::Vertex3f(-1.0f, -1.0f, 0.0f);
		Renderer::TexCoord2f(1.0f, 0.0f); Renderer::Vertex3f(1.0f, -1.0f, 0.0f);
		Renderer::TexCoord2f(1.0f, 1.0f); Renderer::Vertex3f(1.0f, 1.0f, 0.0f);
		Renderer::TexCoord2f(0.0f, 1.0f); Renderer::Vertex3f(-1.0f, 1.0f, 0.0f);
		Renderer::End().render();
	}

	VertexBuffer VertexBuffer::upload() {
		VertexBuffer res;
		res.numVertices = Vertexes;

		int vc = Coordc, tc = Texcoordc, cc = Colorc, nc = Normalc, ac = Attribc;
		int cnt = vc + tc + cc + nc + ac;

		glGenVertexArrays(1, &res.vao);
		glBindVertexArray(res.vao);

		glGenBuffers(1, &res.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, res.vbo);
		glBufferData(GL_ARRAY_BUFFER, Vertexes * (cnt * sizeof(float)), VertexArray, GL_STATIC_DRAW);

		GLuint arrays = 0;
		if (vc > 0) {
			glEnableVertexAttribArray(arrays);
			glVertexAttribPointer(arrays++, vc, GL_FLOAT, GL_FALSE, cnt * sizeof(float), (float*)(0 * sizeof(float)));
		}
		if (tc > 0) {
			glEnableVertexAttribArray(arrays);
			glVertexAttribPointer(arrays++, tc, GL_FLOAT, GL_FALSE, cnt * sizeof(float), (float*)(vc * sizeof(float)));
		}
		if (cc > 0) {
			glEnableVertexAttribArray(arrays);
			glVertexAttribPointer(arrays++, cc, GL_FLOAT, GL_FALSE, cnt * sizeof(float), (float*)((vc + tc) * sizeof(float)));
		}
		if (nc > 0) {
			glEnableVertexAttribArray(arrays);
			glVertexAttribPointer(arrays++, nc, GL_FLOAT, GL_FALSE, cnt * sizeof(float), (float*)((vc + tc + cc) * sizeof(float)));
		}
		if (ac > 0) {
			glEnableVertexAttribArray(arrays);
			glVertexAttribPointer(arrays++, ac, GL_FLOAT, GL_FALSE, cnt * sizeof(float), (float*)((vc + tc + cc + nc) * sizeof(float)));
		}

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
		return res;
	}

	void VertexBuffer::render() const {
		glBindVertexArray(vao);
		glDrawArrays(GL_QUADS, 0, numVertices);
		glBindVertexArray(0);
	}
}

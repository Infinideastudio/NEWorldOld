#include "Renderer.h"

#include <vector>
#include <cmath>
#include "Shader.h"

namespace Renderer {

	GLenum Primitive;
	int Vertexes, Coordc, Texcoordc, Colorc, Normalc, Attribc;
	float* VertexArray = nullptr;
	float Coords[4], TexCoords[4], Colors[4], Normals[4], Attribs[4];
	bool AdvancedRender;
	int ShadowRes = 4096;
	int MaxShadowDistance = 16;
	float sunlightPitch = 30.0f, sunlightHeading = 60.0f;
	vector<Shader> shaders;
	int ActiveShader;
	int index = 0, size = 0;

	const int gBufferCount = 3;
	int gWidth, gHeight;
	Framebuffer shadow, gBuffers, dBuffer;
	bool SoftShadow = false;
	bool VolumetricClouds = false;
	bool AmbientOcclusion = false;

	void Begin(GLenum primitive, int coords, int texCoords, int colors, int normals, int attributes) {
		Primitive = primitive;
		Coordc = coords;
		Texcoordc = texCoords;
		Colorc = colors;
		Normalc = normals;
		Attribc = attributes;
		if (VertexArray == nullptr) VertexArray = new float[ArraySize];
		index = 0;
		Vertexes = 0;
		size = (Coordc + Texcoordc + Colorc + Normalc + Attribc) * 4;
	}

	void addVertex() {
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

	void Vertex2i(int x, int y) { Vertex2f(static_cast<float>(x), static_cast<float>(y)); }
	void Vertex2f(float x, float y) { Coords[0] = x; Coords[1] = y; addVertex(); }
	void Vertex3f(float x, float y, float z) { Coords[0] = x; Coords[1] = y; Coords[2] = z; addVertex(); }
	void TexCoord2f(float x, float y) { TexCoords[0] = x; TexCoords[1] = y; }
	void TexCoord3f(float x, float y, float z) { TexCoords[0] = x; TexCoords[1] = y; TexCoords[2] = z; }
	void Color3f(float r, float g, float b) { Colors[0] = r; Colors[1] = g; Colors[2] = b; }
	void Color4f(float r, float g, float b, float a) { Colors[0] = r; Colors[1] = g; Colors[2] = b; Colors[3] = a; }
	void Normal3f(float x, float y, float z) { Normals[0] = x; Normals[1] = y; Normals[2] = z; }
	void Attrib1f(float a) { Attribs[0] = a; }

	GLuint getNoiseTexture() {
		static GLuint noiseTex = 0;
		if (noiseTex == 0) {
			std::unique_ptr<uint8_t[]> a(new uint8_t[256 * 256 * 4]);
			for (int i = 0; i < 256 * 256; i++) a[i * 4] = a[i * 4 + 1] = static_cast<uint8_t>(rnd() * 256);

			const int OffsetX = 37, OffsetY = 17;
			for (int x = 0; x < 256; x++) for (int y = 0; y < 256; y++) {
					int x1 = (x + OffsetX) % 256, y1 = (y + OffsetY) % 256;
					a[(y * 256 + x) * 4 + 2] = a[(y1 * 256 + x1) * 4];
					a[(y * 256 + x) * 4 + 3] = a[(y1 * 256 + x1) * 4 + 1];
				}

			glGenTextures(1, &noiseTex);
			glBindTexture(GL_TEXTURE_2D, noiseTex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, a.get());
		}
		return noiseTex;
	}

	void initShaders(bool reload) {
		if (shaders.empty() || reload) {
			std::set<string> defines;
			if (MergeFace) defines.insert("MERGE_FACE");
			if (SoftShadow) defines.insert("SOFT_SHADOW");
			if (VolumetricClouds) defines.insert("VOLUMETRIC_CLOUDS");
			if (AmbientOcclusion) defines.insert("AMBIENT_OCCLUSION");
			shaders.clear();
			shaders.push_back(Shader("shaders/ui.vsh", "shaders/ui.fsh", defines));
			shaders.push_back(Shader("shaders/filter.vsh", "shaders/filter.fsh", defines));
			shaders.push_back(Shader("shaders/default.vsh", "shaders/default.fsh", defines));
			shaders.push_back(Shader("shaders/opaque.vsh", "shaders/opaque.fsh", defines));
			shaders.push_back(Shader("shaders/translucent.vsh", "shaders/translucent.fsh", defines));
			shaders.push_back(Shader("shaders/final.vsh", "shaders/final.fsh", defines));
			shaders.push_back(Shader("shaders/shadow.vsh", "shaders/shadow.fsh", defines));
			shaders.push_back(Shader("shaders/debug_shadow.vsh", "shaders/debug_shadow.fsh", defines));
		}

		// Create framebuffers
		gWidth = WindowWidth;
		gHeight = WindowHeight;
		shadow = Framebuffer(ShadowRes, ShadowRes, 0, true, true);
		gBuffers = Framebuffer(gWidth, gHeight, gBufferCount, true, false);
		dBuffer = Framebuffer(gWidth, gHeight, 0, true, false);

		// Set constant uniforms
		shaders[UIShader].bind();
		shaders[UIShader].setUniformI("u_texture_array", 0);
		shaders[UIShader].setUniform("u_buffer_width", float(WindowWidth));
		shaders[UIShader].setUniform("u_buffer_height", float(WindowHeight));

		shaders[DefaultShader].bind();
		shaders[DefaultShader].setUniformI("u_diffuse", 0);

		shaders[OpqaueShader].bind();
		shaders[OpqaueShader].setUniformI("u_diffuse", 0);

		shaders[TranslucentShader].bind();
		shaders[TranslucentShader].setUniformI("u_diffuse", 0);

		float fisheyeFactor = 0.8f;
		shaders[FinalShader].bind();
		shaders[FinalShader].setUniformI("u_diffuse_buffer", 0);
		shaders[FinalShader].setUniformI("u_normal_buffer", 1);
		shaders[FinalShader].setUniformI("u_material_buffer", 2);
		shaders[FinalShader].setUniformI("u_depth_buffer", gBufferCount + 0);
		shaders[FinalShader].setUniformI("u_shadow_texture", gBufferCount + 1);
		shaders[FinalShader].setUniformI("u_noise_texture", gBufferCount + 2);
		shaders[FinalShader].setUniform("u_buffer_width", float(gWidth));
		shaders[FinalShader].setUniform("u_buffer_height", float(gHeight));
		shaders[FinalShader].setUniform("u_shadow_texture_resolution", float(ShadowRes));
		shaders[FinalShader].setUniform("u_shadow_fisheye_factor", fisheyeFactor);
		shaders[FinalShader].setUniform("u_shadow_distance", getShadowDistance() * 16.0f);
		shaders[FinalShader].setUniform("u_render_distance", RenderDistance * 16.0f);

		shaders[ShadowShader].bind();
		shaders[ShadowShader].setUniformI("u_diffuse", 0);
		shaders[ShadowShader].setUniform("u_shadow_fisheye_factor", fisheyeFactor);

		Shader::unbind();
	}

	Mat4f getShadowMatrix() {
		float length = getShadowDistance() * 16.0f;
		auto res = Mat4f(1.0f);
		res = Mat4f::rotation(-sunlightHeading, { 0.0f, 1.0f, 0.0f }) * res;
		res = Mat4f::rotation(sunlightPitch, { 1.0f, 0.0f, 0.0f })* res;
		res = Mat4f::ortho(-length, length, -length, length, -1000.0f, 1000.0f) * res;
		return res;
	}

	Mat4f getShadowMatrixExperimental(float fov, float aspect, double heading, double pitch) {
		float length = getShadowDistance() * 16.0f;
		auto res = Mat4f(1.0f);
		res = Mat4f::rotation(-sunlightHeading, { 0.0f, 1.0f, 0.0f }) * res;
		res = Mat4f::rotation(sunlightPitch, { 1.0f, 0.0f, 0.0f }) * res;

		// Calculate view direction in light space, then rotate it to right (+1, 0)
		auto viewRotate = Mat4f(1.0f);
		viewRotate *= Mat4f::rotation(static_cast<float>(heading), Vec3f(0.0f, 1.0f, 0.0f));
		viewRotate *= Mat4f::rotation(-static_cast<float>(pitch), Vec3f(1.0f, 0.0f, 0.0f));
		auto viewDir = (res * viewRotate).transform(Vec3f(0.0f, 0.0f, -1.0f));
		auto viewDirXY = Vec3f(viewDir.x, viewDir.y, 0.0f);
		if (viewDirXY.length() > 0.01f) {
			float radians = std::atan2(viewDir.y, viewDir.x);
			res = Mat4f::rotation(-radians * 180.0f / std::numbers::pi_v<float>, { 0.0f, 0.0f, 1.0f }) * res;
		}

		// Minimal bounding box containing a diamond-shaped convex hull
		// (should approximate the visible parts better than the whole view frustum)
		float halfHeight = std::tan(static_cast<float>(fov) * (std::numbers::pi_v<float> / 180.0f) / 2.0f);
		float halfWidth = halfHeight * static_cast<float>(aspect);
		auto vertices = std::array{
			Vec3f(0.0f, 0.0f, -1.0f),
			Vec3f(-halfWidth, -halfHeight, -1.0f),
			Vec3f(halfWidth, -halfHeight, -1.0f),
			Vec3f(halfWidth, halfHeight, -1.0f),
			Vec3f(-halfWidth, halfHeight, -1.0f),
		};
		auto toLightSpace = res * viewRotate;
		float xmin = 0.0f, xmax = 0.0f, ymin = 0.0f, ymax = 0.0f;
		for (size_t i = 0; i < vertices.size(); i++) {
			vertices[i].normalize();
			vertices[i] *= length;
			vertices[i] = toLightSpace.transform(vertices[i]);
			xmin = std::min(xmin, vertices[i].x);
			xmax = std::max(xmax, vertices[i].x);
			ymin = std::min(ymin, vertices[i].y);
			ymax = std::max(ymax, vertices[i].y);
		}
		xmin = std::max(xmin, -length);
		xmax = std::min(xmax, length);
		ymin = std::max(ymin, -length);
		ymax = std::min(ymax, length);
		res = Mat4f::ortho(xmin, xmax, ymin, ymax, -1000.0f, 1000.0f) * res;
		return res;
	}

	void ClearSGDBuffers() {
		shadow.bindTargets();
		glClearDepth(1.0f);
		glClear(GL_DEPTH_BUFFER_BIT);
		shadow.unbindTarget();

		gBuffers.bindTargets();
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClearDepth(1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		gBuffers.unbindTarget();

		dBuffer.bindTargets();
		glClearDepth(1.0f);
		glClear(GL_DEPTH_BUFFER_BIT);
		dBuffer.unbindTarget();
	}

	void StartShadowPass(const Mat4f& shadowMatrix, float gameTime) {
		assert(AdvancedRender);

		// Bind output target buffers
		shadow.bindTargets();

		// Set dynamic uniforms
		Shader& shader = shaders[ShadowShader];
		bindShader(ShadowShader);
		shader.setUniform("u_mvp", shadowMatrix);
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

	void StartOpaquePass(const Mat4f& viewMatrix, float gameTime) {
		// Bind output target buffers
		if (AdvancedRender) gBuffers.bindTargets();

		// Set dynamic uniforms
		Shader& shader = shaders[AdvancedRender ? OpqaueShader : DefaultShader];
		bindShader(AdvancedRender ? OpqaueShader : DefaultShader);
		shader.setUniform("u_mvp", viewMatrix);
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

	void StartTranslucentPass(const Mat4f& viewMatrix, float gameTime) {
		// Copy the depth component of the G-buffer to the D-buffer, bind output target buffers
		if (AdvancedRender) {
			// gBuffers.copyDepthTexture(dBuffer);
			gBuffers.bindTargets();
		}

		// Set dynamic uniforms
		Shader& shader = shaders[AdvancedRender ? TranslucentShader : DefaultShader];
		bindShader(AdvancedRender ? TranslucentShader : DefaultShader);
		shader.setUniform("u_mvp", viewMatrix);
		shader.setUniform("u_game_time", gameTime);
	}

	void EndTranslucentPass() {
		// Unbind output target buffers
		if (AdvancedRender) gBuffers.unbindTarget();

		// Disable shader
		Shader::unbind();
	}

	void StartFinalPass(double xpos, double ypos, double zpos, const Mat4f& viewMatrix, const Mat4f& shadowMatrix, float gameTime) {
		assert(AdvancedRender);

		// Bind textures to pre-defined slots
		gBuffers.bindColorTextures(0);
		gBuffers.bindDepthTexture(gBufferCount + 0);
		shadow.bindDepthTexture(gBufferCount + 1);
		glActiveTexture(GL_TEXTURE0 + gBufferCount + 2);
		glBindTexture(GL_TEXTURE_2D, getNoiseTexture());
		glActiveTexture(GL_TEXTURE0);

		// Set dynamic uniforms
		int repeat = 25600;
		int ixpos = int(floor(xpos)), iypos = int(floor(ypos)), izpos = int(floor(zpos));
		Vec3f lightdir = (Mat4f::rotation(sunlightHeading, Vec3f(0, 1, 0)) * Mat4f::rotation(-sunlightPitch, Vec3f(1, 0, 0))).transform(Vec3f(0, 0, -1));

		Shader& shader = shaders[FinalShader];
		bindShader(FinalShader);
		shader.setUniform("u_mvp", viewMatrix);
		shader.setUniform("u_shadow_mvp", shadowMatrix);
		shader.setUniform("u_sunlight_dir", lightdir.x, lightdir.y, lightdir.z);
		shader.setUniform("u_game_time", gameTime);
		shader.setUniformI("u_repeat_length", repeat);
		shader.setUniformI("u_player_coord_int", ixpos, iypos, izpos);
		shader.setUniformI("u_player_coord_mod", ixpos % repeat, iypos % repeat, izpos % repeat);
		shader.setUniform("u_player_coord_frac", float(xpos - ixpos), float(ypos - iypos), float(zpos - izpos));
	}

	void EndFinalPass() {
		assert(AdvancedRender);

		Shader::unbind();
	}

	VertexBuffer VertexBuffer::upload(bool staticDraw) {
		VertexBuffer res;
		res.primitive = Primitive;
		res.numVertices = Vertexes;

		int vc = Coordc, tc = Texcoordc, cc = Colorc, nc = Normalc, ac = Attribc;
		int cnt = vc + tc + cc + nc + ac;

		glGenVertexArrays(1, &res.vao);
		glBindVertexArray(res.vao);

		glGenBuffers(1, &res.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, res.vbo);
		glBufferData(GL_ARRAY_BUFFER, Vertexes * (cnt * sizeof(float)), VertexArray, staticDraw ? GL_STATIC_DRAW : GL_STREAM_DRAW);

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
		return res;
	}

	void VertexBuffer::render() const {
		glBindVertexArray(vao);
		glDrawArrays(primitive, 0, numVertices);
	}
}

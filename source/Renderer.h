#pragma once
#include "Definitions.h"
#include "FrustumTest.h"
#include "Shader.h"
#include "FrameBuffer.h"

namespace Renderer {
	enum Shaders {
		UIShader, FilterShader,
		DefaultShader, OpqaueShader, TranslucentShader, FinalShader, ShadowShader, DebugShadowShader
	};

	const int ArraySize = 4194304;
	extern int size;
	extern int Vertexes;
	extern bool AdvancedRender;
	extern int ShadowRes;
	extern int MaxShadowDistance;
	extern double sunlightPitch;
	extern double sunlightHeading;
	extern vector<Shader> shaders;
	extern int ActiveShader;

	extern Framebuffer shadow;
	extern bool SoftShadow;
	extern bool VolumetricClouds;
	extern bool AmbientOcclusion;

	class VertexBuffer {
	public:
		VertexBuffer() : vao(0), vbo(0), primitive(GL_TRIANGLES), numVertices(0) {}
		VertexBuffer(VertexBuffer const&) = delete;
		VertexBuffer(VertexBuffer&& from) noexcept : VertexBuffer() {
			swap(*this, from);
		}
		VertexBuffer& operator=(VertexBuffer const&) = delete;
		VertexBuffer& operator=(VertexBuffer&& from) noexcept {
			swap(*this, from);
			return *this;
		}

		~VertexBuffer() {
			if (vbo != 0) glDeleteBuffers(1, &vbo);
			if (vao != 0) glDeleteVertexArrays(1, &vao);
		}
		
		friend void swap(VertexBuffer& first, VertexBuffer& second) {
			using std::swap;
			swap(first.vao, second.vao);
			swap(first.vbo, second.vbo);
			swap(first.primitive, second.primitive);
			swap(first.numVertices, second.numVertices);
		}

		static VertexBuffer upload(bool staticDraw);
		bool empty() const { return vao == 0 || vbo == 0 || numVertices == 0; }
		void render() const;

	private:
		GLuint vao;
		GLuint vbo;
		GLenum primitive;
		GLuint numVertices;
	};

	void Begin(GLenum primitive, int coords, int texCoords, int colors, int normals = 0, int attributes = 0);
	void Vertex2i(int x, int y);
	void Vertex2f(float x, float y);
	void Vertex3f(float x, float y, float z);
	void TexCoord2f(float x, float y);
	void TexCoord3f(float x, float y, float z);
	void Color3f(float r, float g, float b);
	void Color4f(float r, float g, float b, float a);
	void Normal3f(float x, float y, float z);
	void Attrib1f(float attr);
	inline VertexBuffer End(bool staticDraw = false) {
		return VertexBuffer::upload(staticDraw);
	}

	void initShaders(bool reload = false);
	inline void bindShader(int shaderID) {
		shaders[shaderID].bind();
		ActiveShader = shaderID;
	}

	inline int getShadowDistance() { return std::min(MaxShadowDistance, RenderDistance); }
	Mat4f getShadowMatrix();
	Mat4f getShadowMatrixExperimental(float fov, float aspect, double heading, double pitch);

	void ClearSGDBuffers();
	void StartShadowPass(const Mat4f& shadowMatrix, float gameTime);
	void EndShadowPass();
	void StartOpaquePass(const Mat4f& viewMatrix, float gametime);
	void EndOpaquePass();
	void StartTranslucentPass(const Mat4f& viewMatrix, float gametime);
	void EndTranslucentPass();
	void StartFinalPass(double xpos, double ypos, double zpos, const Mat4f& viewMatrix, const Mat4f& shadowMatrix, float gameTime);
	void EndFinalPass();
}

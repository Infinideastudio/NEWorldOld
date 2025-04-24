#pragma once
#include "Definitions.h"
#include "GLProc.h"
#include "FrustumTest.h"
#include "Shader.h"
#include "Frustum.h"
#include "FrameBuffer.h"

namespace Renderer {
	enum Shaders {
		DefaultShader, OpqaueShader, TranslucentShader, FinalShader, ShadowShader, DebugShadowShader
	};

	const int ArraySize = 4194304;
	extern int size;
	extern int Vertexes;
	extern bool AdvancedRender;
	extern int ShadowRes;
	extern int MaxShadowDist;
	extern float sunlightXrot, sunlightYrot;
	extern Frustum playerFrustum;
	extern vector<Shader> shaders;
	extern int ActiveShader;

	extern FrameBuffer shadow;
	extern bool VolumetricClouds;

	class VertexBuffer {
	public:
		VertexBuffer() : vao(0), vbo(0), numVertices(0) {}
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
			swap(first.numVertices, second.numVertices);
		}

		static VertexBuffer upload();
		bool empty() const { return numVertices == 0; }
		void render() const;

	private:
		GLuint vao;
		GLuint vbo;
		GLuint numVertices;
	};

	void Begin(int tc, int cc, int nc = 0, int ac = 0);
	void Vertex3f(float x, float y, float z);
	void TexCoord2f(float x, float y);
	void TexCoord3f(float x, float y, float z);
	void Color3f(float r, float g, float b);
	void Color4f(float r, float g, float b, float a);
	void Normal3f(float x, float y, float z);
	void Attrib1f(float attr);
	inline VertexBuffer End() {
		return VertexBuffer::upload();
	}

	void initShaders();
	inline void bindShader(int shaderID) {
		shaders[shaderID].bind();
		ActiveShader = shaderID;
	}
	void destroyShaders();
	FrustumTest getLightFrustum();
	FrustumTest getShadowMapFrustum(double heading, double pitch, int shadowdist, const FrustumTest& playerFrustum);
	
	void ClearSGDBuffers();
	void StartShadowPass(const FrustumTest& lightFrustum, float gameTime);
	void EndShadowPass();
	void StartOpqauePass(const FrustumTest& viewFrustum, float gametime);
	void EndOpaquePass();
	void StartTranslucentPass(const FrustumTest& viewFrustum, float gametime);
	void EndTranslucentPass();
	void StartFinalPass(double xpos, double ypos, double zpos, double heading, double pitch, const FrustumTest& viewFrustum, float gameTime);
	void EndFinalPass();

	void DrawFullscreen();
}

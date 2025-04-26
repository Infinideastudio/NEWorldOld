#pragma once
#include "StdInclude.h"
#include "GLProc.h"

class Shader {
public:
	Shader() {}
	Shader(string vshPath, string fshPath): Shader(vshPath, fshPath, std::set<string>()) {}
	Shader(string vshPath, string fshPath, std::set<string> defines);
	Shader(Shader const&) = delete;
	Shader(Shader&& from) noexcept : Shader() {
		swap(*this, from);
	}
	Shader& operator=(Shader const&) = delete;
	Shader& operator=(Shader&& from) noexcept {
		swap(*this, from);
		return *this;
	}
	~Shader() {
		if (shaderVertex != 0) glDeleteShader(shaderVertex);
		if (shaderFragment != 0) glDeleteShader(shaderFragment);
		if (shaderProgram != 0) glDeleteProgram(shaderProgram);
	}

	friend void swap(Shader& first, Shader& second) {
		using std::swap;
		swap(first.shaderVertex, second.shaderVertex);
		swap(first.shaderFragment, second.shaderFragment);
		swap(first.shaderProgram, second.shaderProgram);
	}

	inline void bind() { glUseProgram(shaderProgram); }
	static inline void unbind() { glUseProgram(0); }

	bool setUniform(const char* uniform, float value);
	bool setUniform(const char* uniform, float v0, float v1, float v2);
	bool setUniform(const char* uniform, float v0, float v1, float v2, float v3);
	bool setUniform(const char* uniform, const float* value);
	bool setUniformI(const char* uniform, int value);
	bool setUniformI(const char* uniform, int v0, int v1, int v2);

private:
	static GLuint loadShader(string filename, unsigned int mode, std::set<string> defines);
	static void checkCompileErrors(GLuint res, string errorMessage);
	static void checkLinkingErrors(GLuint res, string errorMessage);

	GLuint shaderVertex = 0;
	GLuint shaderFragment = 0;
	GLuint shaderProgram = 0;
};

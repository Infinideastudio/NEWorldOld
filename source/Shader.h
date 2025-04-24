#pragma once
#include "StdInclude.h"
#include "GLProc.h"

class Shader {
public:
	Shader(string vshPath, string fshPath, bool bindLocation = false) :Shader(vshPath, fshPath, bindLocation, std::set<string>()) {}
	Shader(string vshPath, string fshPath, bool bindLocation, std::set<string> defines);

	inline void bind() { glUseProgram(shaderProgram); }
	static inline void unbind() { glUseProgram(0); }
	void release();

	bool setUniform(const char* uniform, float value);
	bool setUniform(const char* uniform, int value);
	bool setUniform(const char* uniform, float v0, float v1, float v2);
	bool setUniform(const char* uniform, float v0, float v1, float v2, float v3);
	bool setUniform(const char* uniform, const float* value);
	bool setUniformI(const char* uniform, int v0, int v1, int v2);

private:
	static GLuint loadShader(string filename, unsigned int mode, std::set<string> defines);
	static void checkCompileErrors(GLuint res, string errorMessage);
	static void checkLinkingErrors(GLuint res, string errorMessage);

	GLuint shaderFragment;
	GLuint shaderVertex;
	GLuint shaderProgram;
};

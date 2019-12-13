#pragma once

#include <set>
#include <string>
#include "stdinclude.h"

class Shader {
public:
    Shader(std::string vshPath, std::string fshPath, bool bindLocation = false) : Shader(vshPath, fshPath, bindLocation,
                                                                                         std::set<std::string>()) {}

    Shader(std::string vshPath, std::string fshPath, bool bindLocation, std::set<std::string> defines);

    void bind() { glUseProgramObjectARB(shaderProgram); }

    static void unbind() { glUseProgramObjectARB(0); }

    void release();

    bool setUniform(const char *uniform, float value);

    bool setUniform(const char *uniform, int value);

    bool setUniform(const char *uniform, float v0, float v1, float v2, float v3);

    bool setUniform(const char *uniform, float *value);

private:
    GLhandleARB loadShader(std::string filename, unsigned int mode, std::set<std::string> defines);

    void checkErrors(GLhandleARB res, int status, std::string errorMessage);

    GLhandleARB shaderFragment;
    GLhandleARB shaderVertex;
    GLhandleARB shaderProgram;
};

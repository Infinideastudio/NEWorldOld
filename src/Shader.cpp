#include "Shader.h"
#include "FunctionsKit.h"

Shader::Shader(string vshPath, string fshPath, std::set<string> defines) {
    shaderVertex = loadShader(vshPath, GL_VERTEX_SHADER, defines);
    shaderFragment = loadShader(fshPath, GL_FRAGMENT_SHADER, defines);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, shaderVertex);
    glAttachShader(shaderProgram, shaderFragment);
    glLinkProgram(shaderProgram);
    checkLinkingErrors(shaderProgram, "Shader linking error!");
}

bool Shader::setUniform(const char* uniform, float value) {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1) return false;
    glUniform1f(loc, value);
    return true;
}

bool Shader::setUniformI(const char* uniform, int value) {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1) return false;
    glUniform1i(loc, value);
    return true;
}

bool Shader::setUniform(const char* uniform, float v0, float v1) {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1) return false;
    glUniform2f(loc, v0, v1);
    return true;
}

bool Shader::setUniformI(const char* uniform, int v0, int v1) {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1) return false;
    glUniform2i(loc, v0, v1);
    return true;
}

bool Shader::setUniform(const char* uniform, float v0, float v1, float v2) {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1) return false;
    glUniform3f(loc, v0, v1, v2);
    return true;
}

bool Shader::setUniformI(const char* uniform, int v0, int v1, int v2) {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1) return false;
    glUniform3i(loc, v0, v1, v2);
    return true;
}

bool Shader::setUniform(const char* uniform, float v0, float v1, float v2, float v3) {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1) return false;
    glUniform4f(loc, v0, v1, v2, v3);
    return true;
}

bool Shader::setUniformI(const char* uniform, int v0, int v1, int v2, int v3) {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1) return false;
    glUniform4i(loc, v0, v1, v2, v3);
    return true;
}

bool Shader::setUniform(const char* uniform, const float* value) {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1) return false;
    glUniformMatrix4fv(loc, 1, GL_TRUE, value);
    return true;
}

GLuint Shader::loadShader(string filename, unsigned int mode, std::set<string> defines) {
    GLuint res;
    std::vector<std::string> lines;
    std::vector<GLchar const*> ptr;
    std::vector<GLint> length;
    std::ifstream filein(filename);
    if (!filein.is_open()) {
        std::stringstream ss;
        ss << "Could not load shader " << filename;
        DebugWarning(ss.str());
        return 0;
    }
    bool defs = false;
    while (!filein.eof()) {
        std::string line;
        std::getline(filein, line);
        line += "\n";
        bool insert_defs = !defs && line.starts_with("#version");
        lines.emplace_back(std::move(line));
        if (insert_defs) {
            defs = true;
            for (auto const& define : defines) {
                lines.emplace_back("#define " + define + "\n");
            }
        }
    }
    for (auto const& line : lines) {
        ptr.emplace_back(line.data());
        length.emplace_back(static_cast<GLint>(line.size()));
    }
    res = glCreateShader(mode);
    glShaderSource(res, static_cast<GLsizei>(ptr.size()), ptr.data(), length.data());
    glCompileShader(res);
    checkCompileErrors(res, "Shader compilation error: " + filename);
    return res;
}

void Shader::checkCompileErrors(GLuint res, string errorMessage) {
    int st = GL_TRUE;
    glGetShaderiv(res, GL_COMPILE_STATUS, &st);
    if (st == GL_FALSE) DebugWarning(errorMessage);
    int infologLength, charsWritten;
    glGetShaderiv(res, GL_INFO_LOG_LENGTH, &infologLength);
    if (infologLength > 1) {
        std::unique_ptr<char[]> infoLog = std::make_unique<char[]>(infologLength + 1);
        glGetShaderInfoLog(res, infologLength + 1, &charsWritten, infoLog.get());
        std::cerr << infoLog.get() << std::endl;
    }
}

void Shader::checkLinkingErrors(GLuint res, string errorMessage) {
    int st = GL_TRUE;
    glGetProgramiv(res, GL_LINK_STATUS, &st);
    if (st == GL_FALSE) DebugWarning(errorMessage);
    int infologLength, charsWritten;
    glGetProgramiv(res, GL_INFO_LOG_LENGTH, &infologLength);
    if (infologLength > 1) {
        std::unique_ptr<char[]> infoLog = std::make_unique<char[]>(infologLength + 1);
        glGetProgramInfoLog(res, infologLength + 1, &charsWritten, infoLog.get());
        std::cerr << infoLog.get() << std::endl;
    }
}

module;

#include <glad/gl.h>
#undef assert

export module shaders;
import std;
import types;
import math;
import globals;

export class Shader {
public:
    Shader() = default;
    Shader(std::string vshPath, std::string fshPath):
        Shader(vshPath, fshPath, std::vector<std::string>()) {}
    Shader(std::string vshPath, std::string fshPath, std::vector<std::string> defines);
    Shader(Shader const&) = delete;
    Shader(Shader&& from) noexcept:
        Shader() {
        swap(*this, from);
    }
    auto operator=(Shader const&) -> Shader& = delete;
    auto operator=(Shader&& from) noexcept -> Shader& {
        swap(*this, from);
        return *this;
    }
    ~Shader() {
        if (shaderVertex != 0)
            glDeleteShader(shaderVertex);
        if (shaderFragment != 0)
            glDeleteShader(shaderFragment);
        if (shaderProgram != 0)
            glDeleteProgram(shaderProgram);
    }

    friend void swap(Shader& first, Shader& second) noexcept {
        using std::swap;
        swap(first.shaderVertex, second.shaderVertex);
        swap(first.shaderFragment, second.shaderFragment);
        swap(first.shaderProgram, second.shaderProgram);
    }

    void bind() {
        glUseProgram(shaderProgram);
    }
    static void unbind() {
        glUseProgram(0);
    }

    auto setUniform(char const* uniform, float value) -> bool;
    auto setUniformI(char const* uniform, int value) -> bool;
    auto setUniform(char const* uniform, float v0, float v1) -> bool;
    auto setUniformI(char const* uniform, int v0, int v1) -> bool;
    auto setUniform(char const* uniform, float v0, float v1, float v2) -> bool;
    auto setUniformI(char const* uniform, int v0, int v1, int v2) -> bool;
    auto setUniform(char const* uniform, float v0, float v1, float v2, float v3) -> bool;
    auto setUniformI(char const* uniform, int v0, int v1, int v2, int v3) -> bool;
    auto setUniform(char const* uniform, float const* value) -> bool;

    auto setUniform(char const* uniform, Vec3f const& value) -> bool {
        return setUniform(uniform, value.x(), value.y(), value.z());
    }
    auto setUniformI(char const* uniform, Vec3i const& value) -> bool {
        return setUniformI(uniform, value.x(), value.y(), value.z());
    }
    auto setUniform(char const* uniform, Mat4f const& value) -> bool {
        return setUniform(uniform, value.data());
    }

private:
    static auto loadShader(std::string filename, unsigned int mode, std::vector<std::string> defines) -> GLuint;
    static void checkCompileErrors(GLuint res, std::string errorMessage);
    static void checkLinkingErrors(GLuint res, std::string errorMessage);

    GLuint shaderVertex = 0;
    GLuint shaderFragment = 0;
    GLuint shaderProgram = 0;
};

Shader::Shader(std::string vshPath, std::string fshPath, std::vector<std::string> defines) {
    shaderVertex = loadShader(vshPath, GL_VERTEX_SHADER, defines);
    shaderFragment = loadShader(fshPath, GL_FRAGMENT_SHADER, defines);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, shaderVertex);
    glAttachShader(shaderProgram, shaderFragment);
    glLinkProgram(shaderProgram);
    checkLinkingErrors(shaderProgram, "Shader linking error!");
}

auto Shader::setUniform(char const* uniform, float value) -> bool {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1)
        return false;
    glUniform1f(loc, value);
    return true;
}

auto Shader::setUniformI(char const* uniform, int value) -> bool {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1)
        return false;
    glUniform1i(loc, value);
    return true;
}

auto Shader::setUniform(char const* uniform, float v0, float v1) -> bool {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1)
        return false;
    glUniform2f(loc, v0, v1);
    return true;
}

auto Shader::setUniformI(char const* uniform, int v0, int v1) -> bool {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1)
        return false;
    glUniform2i(loc, v0, v1);
    return true;
}

auto Shader::setUniform(char const* uniform, float v0, float v1, float v2) -> bool {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1)
        return false;
    glUniform3f(loc, v0, v1, v2);
    return true;
}

auto Shader::setUniformI(char const* uniform, int v0, int v1, int v2) -> bool {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1)
        return false;
    glUniform3i(loc, v0, v1, v2);
    return true;
}

auto Shader::setUniform(char const* uniform, float v0, float v1, float v2, float v3) -> bool {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1)
        return false;
    glUniform4f(loc, v0, v1, v2, v3);
    return true;
}

auto Shader::setUniformI(char const* uniform, int v0, int v1, int v2, int v3) -> bool {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1)
        return false;
    glUniform4i(loc, v0, v1, v2, v3);
    return true;
}

auto Shader::setUniform(char const* uniform, float const* value) -> bool {
    int loc = glGetUniformLocation(shaderProgram, uniform);
    if (loc == -1)
        return false;
    glUniformMatrix4fv(loc, 1, GL_TRUE, value);
    return true;
}

auto Shader::loadShader(std::string filename, unsigned int mode, std::vector<std::string> defines) -> GLuint {
    GLuint res = 0;
    std::vector<std::string> lines;
    std::vector<GLchar const*> ptr;
    std::vector<GLint> length;
    auto filein = std::ifstream(filename);
    if (!filein.is_open()) {
        // spdlog::warn("Could not load shader {}", filename);
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
            for (auto const& define: defines) {
                lines.emplace_back("#define " + define + "\n");
            }
        }
    }
    for (auto const& line: lines) {
        ptr.emplace_back(line.data());
        length.emplace_back(static_cast<GLint>(line.size()));
    }
    res = glCreateShader(mode);
    glShaderSource(res, static_cast<GLsizei>(ptr.size()), ptr.data(), length.data());
    glCompileShader(res);
    checkCompileErrors(res, "Shader compilation error: " + filename);
    return res;
}

void Shader::checkCompileErrors(GLuint res, std::string errorMessage) {
    int st = GL_TRUE;
    glGetShaderiv(res, GL_COMPILE_STATUS, &st);
    if (st == GL_FALSE) {
        // spdlog::warn("Error while loading shader {}", errorMessage);
        int infologLength = 0, charsWritten = 0;
        glGetShaderiv(res, GL_INFO_LOG_LENGTH, &infologLength);
        if (infologLength > 1) {
            auto infoLog = std::make_unique<char[]>(infologLength + 1);
            glGetShaderInfoLog(res, infologLength + 1, &charsWritten, infoLog.get());
            // spdlog::warn("{}", infoLog.get());
        }
    }
}

void Shader::checkLinkingErrors(GLuint res, std::string errorMessage) {
    int st = GL_TRUE;
    glGetProgramiv(res, GL_LINK_STATUS, &st);
    if (st == GL_FALSE) {
        // spdlog::warn("error while linking shaders: {}", errorMessage);
        int infologLength = 0, charsWritten = 0;
        glGetProgramiv(res, GL_INFO_LOG_LENGTH, &infologLength);
        if (infologLength > 1) {
            auto infoLog = std::make_unique<char[]>(infologLength + 1);
            glGetProgramInfoLog(res, infologLength + 1, &charsWritten, infoLog.get());
            // spdlog::warn("{}", infoLog.get());
        }
    }
}

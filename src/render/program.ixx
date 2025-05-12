module;

#include <glad/gl.h>

export module render:program;
import std;
import types;
import debug;
import :types;

namespace render {

// Helper function to delete a valid GL shader object.
void _shader_deleter(GLuint handle) {
    glDeleteShader(handle);
}

// Helper function to delete a valid GL program object.
void _program_deleter(GLuint handle) {
    glDeleteProgram(handle);
}

// Manages a GL shader stage code object.
export class Shader {
public:
    // Possible shader stages per GL 4.3.
    enum class Stage : GLenum {
        COMPUTE = GL_COMPUTE_SHADER,
        VERTEX = GL_VERTEX_SHADER,
        TESSELLATION_CONTROL = GL_TESS_CONTROL_SHADER,
        TESSELLATION_EVALUATION = GL_TESS_EVALUATION_SHADER,
        GEOMETRY = GL_GEOMETRY_SHADER,
        FRAGMENT = GL_FRAGMENT_SHADER,
    };

    // Constructs a `Shader` which owns nothing.
    Shader() noexcept = default;

    // Constructs a `Shader` which owns the given `handle`.
    // The `handle` must be either 0 or a valid GL shader object.
    explicit Shader(GLuint handle, Stage stage) noexcept:
        _handle(handle),
        _stage(stage) {}

    auto get() const noexcept -> GLuint {
        return _handle.get();
    }

    auto stage() const noexcept -> Stage {
        return _stage;
    }

    operator bool() const noexcept {
        return bool(_handle);
    }

    // Creates a shader object from the given source code.
    static auto create(Stage stage, std::string_view source) -> std::expected<Shader, std::string> {
        auto handle = glCreateShader(_stage_to_gl_enum(stage));
        if (handle == 0) {
            return std::unexpected("failed to create shader object");
        }
        auto source_cstr = source.data();
        auto source_length = static_cast<GLint>(source.size());
        glShaderSource(handle, 1, &source_cstr, &source_length);
        glCompileShader(handle);
        auto compile_status = GL_TRUE;
        glGetShaderiv(handle, GL_COMPILE_STATUS, &compile_status);
        if (compile_status == GL_FALSE) {
            glDeleteShader(handle);
            auto info_log_length = GLint{0};
            glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &info_log_length);
            auto info_log = std::string(info_log_length + 1, '\0');
            glGetShaderInfoLog(handle, info_log_length + 1, nullptr, info_log.data());
            info_log.resize(info_log_length);
            return std::unexpected("failed to compile shader: " + info_log);
        }
        return Shader(handle, stage);
    }

private:
    Resource<GLuint, 0, decltype(&_shader_deleter), &_shader_deleter> _handle;
    Stage _stage = Stage::VERTEX;

    static constexpr auto _stage_to_gl_enum(Stage stage) -> GLenum {
        return static_cast<GLenum>(stage);
    }
};

// Manages a GL program object.
export class Program {
public:
    // Constructs a `Program` which owns nothing.
    Program() noexcept = default;

    // Constructs a `Program` which owns the given `handle`.
    // The `handle` must be either 0 or a valid GL program object.
    explicit Program(GLuint handle) noexcept:
        _handle(handle) {}

    auto get() const noexcept -> GLuint {
        return _handle.get();
    }

    operator bool() const noexcept {
        return bool(_handle);
    }

    // Binds the owned program to the GL state.
    void bind() const {
        assert(*this, "binding an uninitialised program");
        glUseProgram(get());
    }

    // Sets the value of an opaque uniform variable (which cannot be put into blocks).
    // This operation binds the program.
    void set_opaque(std::string const& name, size_t value) const {
        assert(*this, "setting uniform on an uninitialised program");
        auto location = glGetUniformLocation(get(), name.c_str());
        if (location != -1) {
            glUseProgram(get());
            glUniform1i(location, static_cast<GLint>(value));
        }
    }

    // Sets the uniform block binding for the given block name.
    void set_uniform_block(std::string const& name, size_t index) const {
        assert(*this, "setting uniform block binding on an uninitialised program");
        auto location = glGetUniformBlockIndex(get(), name.c_str());
        glUniformBlockBinding(get(), location, static_cast<GLuint>(index));
    }

    // Creates a program object by linking the given shader objects.
    // Per GL specification, the shader objects can be deleted immediately after linking.
    static auto create(std::initializer_list<Shader> shaders) -> std::expected<Program, std::string> {
        auto handle = glCreateProgram();
        if (handle == 0) {
            return std::unexpected("failed to create program object");
        }
        for (auto const& shader: shaders) {
            if (!shader) {
                return std::unexpected("trying to link an invalid shader stage");
            }
            glAttachShader(handle, shader.get());
        }
        glLinkProgram(handle);
        auto link_status = GL_TRUE;
        glGetProgramiv(handle, GL_LINK_STATUS, &link_status);
        if (link_status == GL_FALSE) {
            glDeleteProgram(handle);
            auto info_log_length = GLint{0};
            glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &info_log_length);
            auto info_log = std::string(info_log_length + 1, '\0');
            glGetProgramInfoLog(handle, info_log_length + 1, nullptr, info_log.data());
            info_log.resize(info_log_length);
            return std::unexpected("failed to link program: " + info_log);
        }
        return Program(handle);
    }

private:
    Resource<GLuint, 0, decltype(&_program_deleter), &_program_deleter> _handle;
};

// Helper function to load a shader from a file.
export auto load_shader_source(std::filesystem::path const& filename, std::vector<std::string> const& defines = {})
    -> std::expected<std::string, std::string> {
    auto res = std::string();
    auto filein = std::ifstream(filename);
    if (!filein.is_open()) {
        return std::unexpected(std::format("failed to open shader file: {}", filename.string()));
    }
    auto defs = false;
    while (!filein.eof()) {
        auto line = std::string();
        std::getline(filein, line);
        line += "\n";
        bool insert_defs = !defs && line.starts_with("#version");
        res += line;
        if (insert_defs) {
            defs = true;
            for (auto const& define: defines) {
                res += "#define " + define + "\n";
            }
        }
    }
    return res;
}

}

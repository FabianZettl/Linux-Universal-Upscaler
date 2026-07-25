#include "shader_program.h"

#include <GL/glew.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace luu {

namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "[ShaderProgram] Error: could not open " << path << "\n";
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

unsigned int compileStage(GLenum stage, const std::string& source, const std::string& path) {
    unsigned int shader = glCreateShader(stage);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        int len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len));
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        std::cerr << "[ShaderProgram] Error: failed to compile " << path << ":\n"
                   << log.data() << "\n";
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

}  // namespace

ShaderProgram::~ShaderProgram() {
    if (program_) glDeleteProgram(program_);
}

bool ShaderProgram::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertexSrc = readFile(vertexPath);
    std::string fragmentSrc = readFile(fragmentPath);
    if (vertexSrc.empty() || fragmentSrc.empty()) {
        return false;
    }

    unsigned int vs = compileStage(GL_VERTEX_SHADER, vertexSrc, vertexPath);
    unsigned int fs = vs ? compileStage(GL_FRAGMENT_SHADER, fragmentSrc, fragmentPath) : 0;
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    int ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        int len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len));
        glGetProgramInfoLog(program, len, nullptr, log.data());
        std::cerr << "[ShaderProgram] Error: failed to link " << vertexPath << " + "
                   << fragmentPath << ":\n"
                   << log.data() << "\n";
        glDeleteProgram(program);
        return false;
    }

    if (program_) glDeleteProgram(program_);
    program_ = program;
    return true;
}

void ShaderProgram::use() const { glUseProgram(program_); }

void ShaderProgram::setInt(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(program_, name.c_str()), value);
}

}  // namespace luu

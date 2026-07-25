#pragma once

#include <string>

namespace luu {

// Compiles/links a vertex+fragment GLSL program. Requires a current GL
// context for every method, including the destructor.
class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    // Returns false (and logs the compiler/linker error log to stderr) on
    // failure; the program is left unusable (id() == 0) in that case.
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

    void use() const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, float x, float y) const;

    unsigned int id() const { return program_; }

private:
    unsigned int program_ = 0;
};

}  // namespace luu

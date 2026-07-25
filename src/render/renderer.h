#pragma once

namespace luu {

class ShaderProgram;
class Texture;

// Draws a single full-screen triangle (no VBO) with `texture` bound to unit
// 0, using `program`. Requires a current GL context.
class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void init();
    void drawFullscreen(const ShaderProgram& program, const Texture& texture) const;

private:
    unsigned int vao_ = 0;
};

}  // namespace luu

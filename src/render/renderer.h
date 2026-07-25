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

    // Binds prev to unit 0, curr to unit 1; program is expected to declare
    // uPrev/uCurr/uFlipY (see src/shaders/framegen.frag).
    void drawBlend(const ShaderProgram& program, const Texture& prev, const Texture& curr) const;

    // FSR1 EASU/RCAS passes (src/shaders/fsr_easu.frag, fsr_rcas.frag).
    // Take a raw GL texture id rather than a Texture& since the source can
    // be either a Texture (a real captured frame) or a RenderTarget's
    // backing texture (EASU's own output, or a materialized frame-gen
    // blend) - both just need to be bindable.
    void drawFsrEasu(const ShaderProgram& program, unsigned int sourceTextureId, bool flipY,
                      float srcWidth, float srcHeight, float dstWidth, float dstHeight) const;
    void drawFsrRcas(const ShaderProgram& program, unsigned int sourceTextureId,
                      float sharpness) const;

private:
    unsigned int vao_ = 0;
};

}  // namespace luu

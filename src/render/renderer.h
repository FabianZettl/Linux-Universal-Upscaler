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
    // uPrev/uCurr/uFlipY (see src/shaders/framegen.frag). blendFactor: 0 =
    // all prev, 1 = all curr (this tick's position within an N-way cycle).
    void drawBlend(const ShaderProgram& program, const Texture& prev, const Texture& curr,
                    float blendFactor) const;

    // FSR1 EASU/RCAS passes (src/shaders/fsr_easu.frag, fsr_rcas.frag).
    // Take a raw GL texture id rather than a Texture& since the source can
    // be either a Texture (a real captured frame) or a RenderTarget's
    // backing texture (EASU's own output, or a materialized frame-gen
    // blend) - both just need to be bindable.
    void drawFsrEasu(const ShaderProgram& program, unsigned int sourceTextureId, bool flipY,
                      float srcWidth, float srcHeight, float dstWidth, float dstHeight) const;
    void drawFsrRcas(const ShaderProgram& program, unsigned int sourceTextureId,
                      float sharpness) const;

    // Draws a 6-vertex (0,0)-(1,1) unit quad with no vertex buffer (see
    // src/shaders/text.vert) - `program` must already be in use with its
    // uniforms/textures set by the caller (TextRenderer). Reuses the same
    // attributeless VAO the other draw* methods use.
    void drawQuad(const ShaderProgram& program) const;

private:
    unsigned int vao_ = 0;
};

}  // namespace luu

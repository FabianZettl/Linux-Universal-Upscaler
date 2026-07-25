#include "renderer.h"

#include <GL/glew.h>

#include "shader_program.h"
#include "texture.h"

namespace luu {

Renderer::~Renderer() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

void Renderer::init() {
    // No vertex buffer needed - fullscreen.vert derives positions from
    // gl_VertexID. Core profile still requires a bound VAO to draw at all.
    glGenVertexArrays(1, &vao_);
}

void Renderer::drawFullscreen(const ShaderProgram& program, const Texture& texture) const {
    program.use();
    texture.bind(0);
    program.setInt("uSource", 0);
    program.setInt("uFlipY", texture.flipY() ? 1 : 0);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::drawBlend(const ShaderProgram& program, const Texture& prev,
                          const Texture& curr) const {
    program.use();
    prev.bind(0);
    curr.bind(1);
    program.setInt("uPrev", 0);
    program.setInt("uCurr", 1);
    program.setInt("uFlipY", curr.flipY() ? 1 : 0);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::drawFsrEasu(const ShaderProgram& program, unsigned int sourceTextureId, bool flipY,
                            float srcWidth, float srcHeight, float dstWidth,
                            float dstHeight) const {
    program.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTextureId);
    program.setInt("uSource", 0);
    program.setInt("uFlipY", flipY ? 1 : 0);
    program.setVec2("uSrcSize", srcWidth, srcHeight);
    program.setVec2("uDstSize", dstWidth, dstHeight);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::drawFsrRcas(const ShaderProgram& program, unsigned int sourceTextureId,
                            float sharpness) const {
    program.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTextureId);
    program.setInt("uSource", 0);
    program.setFloat("uSharpness", sharpness);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

}  // namespace luu

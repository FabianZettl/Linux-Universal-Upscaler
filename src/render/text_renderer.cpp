#include "text_renderer.h"

#include <GL/glew.h>

#include <vector>

#include "bitmap_font.h"
#include "renderer.h"
#include "shader_program.h"

namespace luu {

namespace {

int glyphIndex(char ch) {
    const auto& glyphs = fontGlyphs();
    for (size_t i = 0; i < glyphs.size(); ++i) {
        if (glyphs[i].ch == ch) return static_cast<int>(i);
    }
    return -1;  // unsupported character - caller treats like a space
}

}  // namespace

TextRenderer::TextRenderer() {
    const auto& glyphs = fontGlyphs();
    const int atlasWidth = static_cast<int>(glyphs.size()) * kFontGlyphWidth;
    const int atlasHeight = kFontGlyphHeight;

    std::vector<uint8_t> pixels(static_cast<size_t>(atlasWidth) * atlasHeight, 0);
    for (size_t gi = 0; gi < glyphs.size(); ++gi) {
        for (int row = 0; row < kFontGlyphHeight; ++row) {
            uint8_t bits = glyphs[gi].rows[row];
            for (int col = 0; col < kFontGlyphWidth; ++col) {
                bool lit = (bits >> (kFontGlyphWidth - 1 - col)) & 1;
                int x = static_cast<int>(gi) * kFontGlyphWidth + col;
                pixels[static_cast<size_t>(row) * atlasWidth + x] = lit ? 255 : 0;
            }
        }
    }

    glGenTextures(1, &atlasTexture_);
    glBindTexture(GL_TEXTURE_2D, atlasTexture_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlasWidth, atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE,
                 pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

TextRenderer::~TextRenderer() {
    if (atlasTexture_) glDeleteTextures(1, &atlasTexture_);
}

void TextRenderer::drawText(Renderer& renderer, const ShaderProgram& program,
                             const std::string& text, float x, float y, float scale,
                             float screenWidth, float screenHeight, float r, float g,
                             float b) const {
    const int glyphCount = static_cast<int>(fontGlyphs().size());
    const float atlasWidth = static_cast<float>(glyphCount * kFontGlyphWidth);
    const float glyphW = kFontGlyphWidth * scale;
    const float glyphH = kFontGlyphHeight * scale;
    const float advance = (kFontGlyphWidth + 1) * scale;

    program.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture_);
    program.setInt("uFontAtlas", 0);
    program.setVec2("uScreenSize", screenWidth, screenHeight);
    program.setVec3("uColor", r, g, b);

    float cursorX = x;
    for (char ch : text) {
        int idx = glyphIndex(ch);
        if (idx >= 0) {
            float u0 = static_cast<float>(idx * kFontGlyphWidth) / atlasWidth;
            float u1 = static_cast<float>((idx + 1) * kFontGlyphWidth) / atlasWidth;
            program.setVec4("uRectPx", cursorX, y, glyphW, glyphH);
            program.setVec4("uUVRect", u0, 0.0f, u1, 1.0f);
            renderer.drawQuad(program);
        }
        cursorX += advance;
    }
}

}  // namespace luu

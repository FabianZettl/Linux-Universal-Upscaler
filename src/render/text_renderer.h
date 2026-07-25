#pragma once

#include <string>

namespace luu {

class Renderer;
class ShaderProgram;

// Draws text using the tiny built-in 5x7 bitmap font (bitmap_font.h) via
// a procedurally-built font atlas texture - no external font asset or
// library. Requires a current GL context for every method, including the
// destructor.
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    // program must be linked from text.vert/text.frag. x/y are the
    // top-left corner in pixels (top-left origin, y-down); scale is an
    // integer pixel multiplier (2 = each font pixel becomes a 2x2 block).
    // Unsupported characters (outside bitmap_font.h's set) are skipped
    // but still advance the cursor as a space.
    void drawText(Renderer& renderer, const ShaderProgram& program, const std::string& text,
                  float x, float y, float scale, float screenWidth, float screenHeight,
                  float r = 1.0f, float g = 1.0f, float b = 1.0f) const;

private:
    unsigned int atlasTexture_ = 0;
};

}  // namespace luu

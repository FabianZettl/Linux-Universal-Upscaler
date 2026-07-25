#pragma once

namespace luu {

// An offscreen FBO + RGBA8 texture pair, sized once at construction.
// Used as the intermediate between multi-pass shader pipelines (e.g.
// FSR's EASU output feeding into RCAS) - Renderer::drawFullscreen/
// drawBlend/drawFsrEasu/drawFsrRcas all just draw to whatever framebuffer
// is currently bound, so RenderTarget::bind() before one of those calls
// redirects its output here instead of the window. Requires a current GL
// context for every method, including the destructor.
class RenderTarget {
public:
    RenderTarget(int width, int height);
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    // Binds the FBO and sets the viewport to its size.
    void bind() const;

    unsigned int textureId() const { return texture_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    unsigned int fbo_ = 0;
    unsigned int texture_ = 0;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace luu

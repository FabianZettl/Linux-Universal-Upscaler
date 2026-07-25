#include "texture.h"

#include <GL/glew.h>

namespace luu {

Texture::Texture() { glGenTextures(1, &id_); }

Texture::~Texture() {
    if (id_) glDeleteTextures(1, &id_);
}

void Texture::uploadBGRA(const CaptureFrame& frame, FilterMode filter) {
    // glTexImage2D places row 0 of the source data at texture V=0, but our
    // full-screen quad samples V=0 at the bottom of the screen - so normal
    // (non-inverted) top-to-bottom row data needs a V-flip to display
    // right-side up. If the compositor already handed us bottom-to-top
    // (y_invert) data, it lines up with GL's convention as-is.
    flipY_ = !frame.yInverted;
    glBindTexture(GL_TEXTURE_2D, id_);

    // stride may include padding beyond width*4 bytes; tell GL the true row
    // length (in texels) so it doesn't read the pixels skewed.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(frame.stride / 4));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(frame.width),
                 static_cast<GLsizei>(frame.height), 0, GL_BGRA, GL_UNSIGNED_BYTE,
                 frame.pixels.data());
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    GLint glFilter = (filter == FilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void Texture::bind(unsigned int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id_);
}

}  // namespace luu

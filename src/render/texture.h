#pragma once

#include "capture_base.h"

namespace luu {

enum class FilterMode { Nearest, Linear };

// A single GL_TEXTURE_2D populated from a captured frame. Requires a
// current GL context for every method, including the destructor.
class Texture {
public:
    Texture();
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // frame.pixels is expected to be tightly packed BGRA8 (see capture_base.h).
    void uploadBGRA(const CaptureFrame& frame, FilterMode filter);

    void bind(unsigned int unit = 0) const;

    unsigned int id() const { return id_; }
    bool flipY() const { return flipY_; }

private:
    unsigned int id_ = 0;
    bool flipY_ = false;
};

}  // namespace luu

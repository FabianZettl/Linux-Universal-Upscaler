#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace luu {

// A single captured frame, tightly packed BGRA8 bytes (matches wl_shm
// ARGB8888/XRGB8888 byte order on little-endian systems, i.e. directly
// uploadable as GL_BGRA/GL_UNSIGNED_BYTE).
struct CaptureFrame {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    std::vector<uint8_t> pixels;
    // True if row 0 of `pixels` is the bottom of the image, not the top
    // (wlr-screencopy's y_invert flag - compositor/driver dependent).
    bool yInverted = false;
};

// Implemented per capture method (Wayland screencopy now, X11 later).
class ICaptureBackend {
public:
    virtual ~ICaptureBackend() = default;

    // False if setup failed (compositor missing a required protocol,
    // capture target not found, etc.) - already logged to stderr by the
    // implementation. captureFrame() will always fail if this is false.
    virtual bool isSupported() const = 0;

    // Returns nullopt (and logs to stderr) on failure.
    virtual std::optional<CaptureFrame> captureFrame() = 0;
};

}  // namespace luu

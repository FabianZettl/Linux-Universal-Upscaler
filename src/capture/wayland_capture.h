#pragma once

#include <memory>

#include "capture_base.h"

namespace luu {

// Captures a still frame of the first output it finds via the
// wlr-screencopy-unstable-v1 protocol (wlroots-based compositors only,
// e.g. Hyprland, Sway).
//
// Opens and owns its own wl_display connection, independent of whatever
// windowing backend the renderer uses (GLEW requires a GLX context, so the
// render window goes through XWayland/GLX - this capture path stays on a
// separate, native Wayland connection so it still works regardless).
class WaylandScreencopyCapture : public ICaptureBackend {
public:
    WaylandScreencopyCapture();
    ~WaylandScreencopyCapture() override;

    WaylandScreencopyCapture(const WaylandScreencopyCapture&) = delete;
    WaylandScreencopyCapture& operator=(const WaylandScreencopyCapture&) = delete;

    // True if the compositor advertised zwlr_screencopy_manager_v1 and
    // wl_shm. If false, captureFrame() will always fail.
    bool isSupported() const;

    std::optional<CaptureFrame> captureFrame() override;

    // Public so the .cpp's free-function Wayland listener callbacks (which
    // aren't members/friends) can name it for the static_cast<Impl*> from
    // the listener's void* userdata; the definition itself stays local to
    // wayland_capture.cpp, so nothing is actually exposed to callers.
    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace luu

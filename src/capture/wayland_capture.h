#pragma once

#include <memory>
#include <string>

#include "capture_base.h"

namespace luu {

// Captures frames of a wl_output via the wlr-screencopy-unstable-v1
// protocol (wlroots-based compositors only, e.g. Hyprland, Sway).
// captureFrame() can be called repeatedly (e.g. once per render frame);
// each call fully allocates and tears down its own shm buffer.
//
// Opens and owns its own wl_display connection, independent of whatever
// windowing backend the renderer uses (GLEW requires a GLX context, so the
// render window goes through XWayland/GLX - this capture path stays on a
// separate, native Wayland connection so it still works regardless).
class WaylandScreencopyCapture : public ICaptureBackend {
public:
    // preferredOutputName: exact wl_output name (e.g. "DP-2", see
    // `hyprctl monitors` / `wlr-randr`) to capture. Empty picks the first
    // output the compositor advertises (logged to stderr) - if that
    // happens to be the output showing the preview window, the capture
    // will recursively include the window itself (self-capture mirror).
    // If non-empty but no output has that name, isSupported() is false and
    // the available names are logged to stderr.
    explicit WaylandScreencopyCapture(const std::string& preferredOutputName = "");
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

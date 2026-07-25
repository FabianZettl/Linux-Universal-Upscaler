#pragma once

#include <memory>
#include <string>
#include <vector>

#include "capture_base.h"

namespace luu {

struct WindowInfo {
    std::string identifier;  // stable only while the window stays mapped - see .cpp
    std::string title;
    std::string appId;
};

// Captures a specific window (not a whole output) via the standardized
// ext-foreign-toplevel-list-v1 / ext-image-capture-source-v1 /
// ext-image-copy-capture-v1 protocols - portable to any compositor that
// implements them, unlike the Hyprland-specific toplevel-export protocol.
//
// A window's `identifier` (see listWindows()) is only valid while that
// window stays open - closing and reopening it (even the "same" window
// again) yields a different identifier. Callers should re-list and
// re-pick rather than assume a stored identifier stays valid indefinitely.
class WaylandToplevelCapture : public ICaptureBackend {
public:
    // One-shot query: connects, enumerates currently open toplevels,
    // disconnects. Used by `luu_capture_preview --list-windows` - doesn't
    // require a full capture session.
    static std::vector<WindowInfo> listWindows();

    // windowIdentifier: an identifier from listWindows(). If no currently
    // open window matches, isSupported() is false and the available
    // windows are logged to stderr (the window most likely closed since
    // it was picked - see class comment).
    explicit WaylandToplevelCapture(const std::string& windowIdentifier);
    ~WaylandToplevelCapture() override;

    WaylandToplevelCapture(const WaylandToplevelCapture&) = delete;
    WaylandToplevelCapture& operator=(const WaylandToplevelCapture&) = delete;

    bool isSupported() const override;

    std::optional<CaptureFrame> captureFrame() override;

    struct Impl;  // public for the .cpp's free-function listener callbacks; see wayland_capture.h

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace luu

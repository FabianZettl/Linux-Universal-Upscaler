#include "wayland_toplevel_capture.h"

#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include <ext-foreign-toplevel-list-v1-client-protocol.h>
#include <ext-image-capture-source-v1-client-protocol.h>
#include <ext-image-copy-capture-v1-client-protocol.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>

namespace luu {

namespace {

struct ToplevelEntry {
    ext_foreign_toplevel_handle_v1* handle = nullptr;
    std::string title;
    std::string appId;
    std::string identifier;
};

struct Globals {
    wl_shm* shm = nullptr;
    ext_foreign_toplevel_list_v1* list = nullptr;
    ext_foreign_toplevel_image_capture_source_manager_v1* sourceManager = nullptr;
    ext_image_copy_capture_manager_v1* captureManager = nullptr;
    std::vector<ToplevelEntry>* entries = nullptr;  // filled in as `toplevel` events arrive
};

void handleTitle(void* data, ext_foreign_toplevel_handle_v1*, const char* title) {
    static_cast<ToplevelEntry*>(data)->title = title;
}
void handleAppId(void* data, ext_foreign_toplevel_handle_v1*, const char* appId) {
    static_cast<ToplevelEntry*>(data)->appId = appId;
}
void handleIdentifier(void* data, ext_foreign_toplevel_handle_v1*, const char* identifier) {
    static_cast<ToplevelEntry*>(data)->identifier = identifier;
}
void handleDone(void*, ext_foreign_toplevel_handle_v1*) {}
void handleClosed(void*, ext_foreign_toplevel_handle_v1*) {}

constexpr ext_foreign_toplevel_handle_v1_listener kHandleListener = {
    .closed = handleClosed,
    .done = handleDone,
    .title = handleTitle,
    .app_id = handleAppId,
    .identifier = handleIdentifier,
};

void listToplevel(void* data, ext_foreign_toplevel_list_v1*, ext_foreign_toplevel_handle_v1* handle) {
    auto* globals = static_cast<Globals*>(data);
    globals->entries->push_back(ToplevelEntry{handle, "", "", ""});
    ext_foreign_toplevel_handle_v1_add_listener(handle, &kHandleListener, &globals->entries->back());
}
void listFinished(void*, ext_foreign_toplevel_list_v1*) {}

constexpr ext_foreign_toplevel_list_v1_listener kListListener = {
    .toplevel = listToplevel,
    .finished = listFinished,
};

void registryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface,
                     uint32_t version) {
    auto* globals = static_cast<Globals*>(data);
    if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        globals->shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (std::strcmp(interface, ext_foreign_toplevel_list_v1_interface.name) == 0) {
        globals->list = static_cast<ext_foreign_toplevel_list_v1*>(wl_registry_bind(
            registry, name, &ext_foreign_toplevel_list_v1_interface, 1));
        ext_foreign_toplevel_list_v1_add_listener(globals->list, &kListListener, globals);
    } else if (std::strcmp(interface, ext_foreign_toplevel_image_capture_source_manager_v1_interface.name) ==
               0) {
        globals->sourceManager = static_cast<ext_foreign_toplevel_image_capture_source_manager_v1*>(
            wl_registry_bind(registry, name,
                              &ext_foreign_toplevel_image_capture_source_manager_v1_interface, 1));
    } else if (std::strcmp(interface, ext_image_copy_capture_manager_v1_interface.name) == 0) {
        globals->captureManager = static_cast<ext_image_copy_capture_manager_v1*>(wl_registry_bind(
            registry, name, &ext_image_copy_capture_manager_v1_interface, 1));
    }
}
void registryGlobalRemove(void*, wl_registry*, uint32_t) {}

constexpr wl_registry_listener kRegistryListener = {
    .global = registryGlobal,
    .global_remove = registryGlobalRemove,
};

// Connects, binds every global this file needs, and enumerates currently
// open toplevels. Three roundtrips: registry binds, the list's `toplevel`
// burst (creates handle objects + attaches their listeners), then each
// handle's own title/app_id/identifier events - the same conservative
// "bind now, properties next roundtrip" shape wayland_capture.cpp already
// uses for wl_output, one level deeper since toplevels are enumerated via
// a burst of events rather than direct registry binds.
struct Connection {
    wl_display* display = nullptr;
    wl_registry* registry = nullptr;
    Globals globals;
    std::vector<ToplevelEntry> entries;

    bool connect() {
        display = wl_display_connect(nullptr);
        if (!display) {
            std::cerr << "[WaylandToplevelCapture] Error: could not connect to a Wayland "
                         "display (is WAYLAND_DISPLAY set?)\n";
            return false;
        }
        globals.entries = &entries;
        registry = wl_display_get_registry(display);
        wl_registry_add_listener(registry, &kRegistryListener, &globals);
        wl_display_roundtrip(display);  // registry globals + binds
        if (!globals.list) {
            std::cerr << "[WaylandToplevelCapture] Error: compositor does not support "
                         "ext_foreign_toplevel_list_v1\n";
            return false;
        }
        wl_display_roundtrip(display);  // the list's `toplevel` burst
        wl_display_roundtrip(display);  // each handle's title/app_id/identifier
        return true;
    }

    ~Connection() {
        if (globals.list) ext_foreign_toplevel_list_v1_destroy(globals.list);
        if (globals.sourceManager)
            ext_foreign_toplevel_image_capture_source_manager_v1_destroy(globals.sourceManager);
        if (globals.captureManager) ext_image_copy_capture_manager_v1_destroy(globals.captureManager);
        if (globals.shm) wl_shm_destroy(globals.shm);
        if (registry) wl_registry_destroy(registry);
        if (display) wl_display_disconnect(display);
    }
};

bool isSupportedShmFormat(uint32_t format) {
    return format == WL_SHM_FORMAT_ARGB8888 || format == WL_SHM_FORMAT_XRGB8888;
}

}  // namespace

std::vector<WindowInfo> WaylandToplevelCapture::listWindows() {
    Connection conn;
    std::vector<WindowInfo> result;
    if (!conn.connect()) return result;
    for (auto& entry : conn.entries) {
        result.push_back(WindowInfo{entry.identifier, entry.title, entry.appId});
        ext_foreign_toplevel_handle_v1_destroy(entry.handle);
    }
    return result;
}

struct WaylandToplevelCapture::Impl {
    wl_display* display = nullptr;
    wl_registry* registry = nullptr;
    Globals globals;
    std::vector<ToplevelEntry> entries;

    ext_foreign_toplevel_handle_v1* targetHandle = nullptr;
    ext_image_capture_source_v1* source = nullptr;
    ext_image_copy_capture_session_v1* session = nullptr;

    uint32_t bufferWidth = 0;
    uint32_t bufferHeight = 0;
    uint32_t shmFormat = 0;
    bool haveConstraints = false;
    bool sessionStopped = false;
};

namespace {

void sessionBufferSize(void* data, ext_image_copy_capture_session_v1*, uint32_t width,
                        uint32_t height) {
    auto* impl = static_cast<WaylandToplevelCapture::Impl*>(data);
    impl->bufferWidth = width;
    impl->bufferHeight = height;
}
void sessionShmFormat(void* data, ext_image_copy_capture_session_v1*, uint32_t format) {
    auto* impl = static_cast<WaylandToplevelCapture::Impl*>(data);
    if (isSupportedShmFormat(format)) impl->shmFormat = format;
}
void sessionDmabufDevice(void*, ext_image_copy_capture_session_v1*, wl_array*) {}
void sessionDmabufFormat(void*, ext_image_copy_capture_session_v1*, uint32_t, wl_array*) {}
void sessionDone(void* data, ext_image_copy_capture_session_v1*) {
    static_cast<WaylandToplevelCapture::Impl*>(data)->haveConstraints = true;
}
void sessionStopped(void* data, ext_image_copy_capture_session_v1*) {
    static_cast<WaylandToplevelCapture::Impl*>(data)->sessionStopped = true;
}

constexpr ext_image_copy_capture_session_v1_listener kSessionListener = {
    .buffer_size = sessionBufferSize,
    .shm_format = sessionShmFormat,
    .dmabuf_device = sessionDmabufDevice,
    .dmabuf_format = sessionDmabufFormat,
    .done = sessionDone,
    .stopped = sessionStopped,
};

struct FrameState {
    bool ready = false;
    bool failed = false;
    bool yInverted = false;
};

void frameTransform(void* data, ext_image_copy_capture_frame_v1*, uint32_t transform) {
    auto* state = static_cast<FrameState*>(data);
    if (transform == WL_OUTPUT_TRANSFORM_NORMAL) {
        state->yInverted = false;
    } else if (transform == WL_OUTPUT_TRANSFORM_FLIPPED) {
        state->yInverted = true;
    } else {
        std::cerr << "[WaylandToplevelCapture] Note: unsupported buffer transform " << transform
                   << ", orientation may be off\n";
    }
}
void frameDamage(void*, ext_image_copy_capture_frame_v1*, int32_t, int32_t, int32_t, int32_t) {}
void framePresentationTime(void*, ext_image_copy_capture_frame_v1*, uint32_t, uint32_t, uint32_t) {}
void frameReady(void* data, ext_image_copy_capture_frame_v1*) {
    static_cast<FrameState*>(data)->ready = true;
}
void frameFailed(void* data, ext_image_copy_capture_frame_v1*, uint32_t) {
    static_cast<FrameState*>(data)->failed = true;
}

constexpr ext_image_copy_capture_frame_v1_listener kFrameListener = {
    .transform = frameTransform,
    .damage = frameDamage,
    .presentation_time = framePresentationTime,
    .ready = frameReady,
    .failed = frameFailed,
};

}  // namespace

WaylandToplevelCapture::WaylandToplevelCapture(const std::string& windowIdentifier)
    : impl_(std::make_unique<Impl>()) {
    impl_->display = wl_display_connect(nullptr);
    if (!impl_->display) {
        std::cerr << "[WaylandToplevelCapture] Error: could not connect to a Wayland display "
                     "(is WAYLAND_DISPLAY set?)\n";
        return;
    }

    impl_->globals.entries = &impl_->entries;
    impl_->registry = wl_display_get_registry(impl_->display);
    wl_registry_add_listener(impl_->registry, &kRegistryListener, &impl_->globals);
    wl_display_roundtrip(impl_->display);  // registry globals + binds

    if (!impl_->globals.list || !impl_->globals.shm || !impl_->globals.sourceManager ||
        !impl_->globals.captureManager) {
        std::cerr << "[WaylandToplevelCapture] Error: compositor does not support per-window "
                     "capture (needs ext_foreign_toplevel_list_v1, "
                     "ext_foreign_toplevel_image_capture_source_manager_v1, "
                     "ext_image_copy_capture_manager_v1)\n";
        return;
    }

    wl_display_roundtrip(impl_->display);  // the list's `toplevel` burst
    wl_display_roundtrip(impl_->display);  // each handle's title/app_id/identifier

    for (auto& entry : impl_->entries) {
        if (entry.identifier == windowIdentifier) {
            impl_->targetHandle = entry.handle;
        } else {
            ext_foreign_toplevel_handle_v1_destroy(entry.handle);
        }
    }

    if (!impl_->targetHandle) {
        std::cerr << "[WaylandToplevelCapture] Error: no open window with identifier '"
                   << windowIdentifier << "' (it may have been closed - pick a window again). "
                   << "Currently open: ";
        bool first = true;
        for (auto& entry : impl_->entries) {
            if (entry.identifier == windowIdentifier) continue;
            if (!first) std::cerr << ", ";
            std::cerr << "'" << entry.title << "' (" << entry.identifier << ")";
            first = false;
        }
        std::cerr << "\n";
        return;
    }

    impl_->source = ext_foreign_toplevel_image_capture_source_manager_v1_create_source(
        impl_->globals.sourceManager, impl_->targetHandle);
    impl_->session = ext_image_copy_capture_manager_v1_create_session(
        impl_->globals.captureManager, impl_->source,
        EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS);
    ext_image_copy_capture_session_v1_add_listener(impl_->session, &kSessionListener, impl_.get());

    while (!impl_->haveConstraints && !impl_->sessionStopped) {
        if (wl_display_dispatch(impl_->display) < 0) {
            std::cerr << "[WaylandToplevelCapture] Error: Wayland dispatch failed while waiting "
                         "for session constraints\n";
            return;
        }
    }
    if (impl_->sessionStopped) {
        std::cerr << "[WaylandToplevelCapture] Error: capture session stopped before it was "
                     "ready (window closed?)\n";
    } else if (impl_->shmFormat == 0) {
        std::cerr << "[WaylandToplevelCapture] Error: compositor did not offer a supported shm "
                     "format (ARGB8888/XRGB8888)\n";
    }
}

WaylandToplevelCapture::~WaylandToplevelCapture() {
    if (impl_->session) ext_image_copy_capture_session_v1_destroy(impl_->session);
    if (impl_->source) ext_image_capture_source_v1_destroy(impl_->source);
    if (impl_->targetHandle) ext_foreign_toplevel_handle_v1_destroy(impl_->targetHandle);
    if (impl_->globals.list) ext_foreign_toplevel_list_v1_destroy(impl_->globals.list);
    if (impl_->globals.sourceManager)
        ext_foreign_toplevel_image_capture_source_manager_v1_destroy(impl_->globals.sourceManager);
    if (impl_->globals.captureManager)
        ext_image_copy_capture_manager_v1_destroy(impl_->globals.captureManager);
    if (impl_->globals.shm) wl_shm_destroy(impl_->globals.shm);
    if (impl_->registry) wl_registry_destroy(impl_->registry);
    if (impl_->display) wl_display_disconnect(impl_->display);
}

bool WaylandToplevelCapture::isSupported() const {
    return impl_->session != nullptr && impl_->haveConstraints && !impl_->sessionStopped &&
           impl_->shmFormat != 0;
}

std::optional<CaptureFrame> WaylandToplevelCapture::captureFrame() {
    if (!isSupported()) return std::nullopt;

    ext_image_copy_capture_frame_v1* frame =
        ext_image_copy_capture_session_v1_create_frame(impl_->session);

    const uint32_t width = impl_->bufferWidth;
    const uint32_t height = impl_->bufferHeight;
    const uint32_t stride = width * 4;  // tightly packed 32bpp - the protocol gives no stride event
    const size_t size = static_cast<size_t>(stride) * height;

    int fd = memfd_create("luu-toplevel-capture", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, static_cast<off_t>(size)) != 0) {
        std::cerr << "[WaylandToplevelCapture] Error: failed to create anonymous shm file: "
                   << std::strerror(errno) << "\n";
        if (fd >= 0) close(fd);
        ext_image_copy_capture_frame_v1_destroy(frame);
        return std::nullopt;
    }
    void* map = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        std::cerr << "[WaylandToplevelCapture] Error: mmap failed: " << std::strerror(errno)
                   << "\n";
        close(fd);
        ext_image_copy_capture_frame_v1_destroy(frame);
        return std::nullopt;
    }

    wl_shm_pool* pool = wl_shm_create_pool(impl_->globals.shm, fd, static_cast<int32_t>(size));
    wl_buffer* buffer =
        wl_shm_pool_create_buffer(pool, 0, static_cast<int32_t>(width), static_cast<int32_t>(height),
                                   static_cast<int32_t>(stride), impl_->shmFormat);
    wl_shm_pool_destroy(pool);

    FrameState state;
    ext_image_copy_capture_frame_v1_add_listener(frame, &kFrameListener, &state);
    ext_image_copy_capture_frame_v1_attach_buffer(frame, buffer);
    ext_image_copy_capture_frame_v1_capture(frame);

    while (!state.ready && !state.failed) {
        if (wl_display_dispatch(impl_->display) < 0) {
            std::cerr << "[WaylandToplevelCapture] Error: Wayland dispatch failed while waiting "
                         "for copy completion\n";
            munmap(map, size);
            close(fd);
            wl_buffer_destroy(buffer);
            ext_image_copy_capture_frame_v1_destroy(frame);
            return std::nullopt;
        }
    }

    std::optional<CaptureFrame> result;
    if (state.ready) {
        CaptureFrame out;
        out.width = width;
        out.height = height;
        out.stride = stride;
        out.yInverted = state.yInverted;
        out.pixels.resize(size);
        std::memcpy(out.pixels.data(), map, size);
        result = std::move(out);
    } else {
        std::cerr << "[WaylandToplevelCapture] Error: capture failed this frame (window closed, "
                     "or buffer constraints changed - e.g. the window was resized)\n";
    }

    munmap(map, size);
    close(fd);
    wl_buffer_destroy(buffer);
    ext_image_copy_capture_frame_v1_destroy(frame);
    return result;
}

}  // namespace luu

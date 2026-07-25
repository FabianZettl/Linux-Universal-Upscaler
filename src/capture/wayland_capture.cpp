#include "wayland_capture.h"

#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wlr-screencopy-unstable-v1-client-protocol.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

namespace luu {

struct OutputInfo {
    wl_output* output = nullptr;  // owned by WaylandScreencopyCapture::Impl::outputs
    std::string name;
};

struct WaylandScreencopyCapture::Impl {
    wl_display* display = nullptr;  // owned
    wl_output* output = nullptr;    // selected output - alias into outputs, not separately owned
    wl_registry* registry = nullptr;
    wl_shm* shm = nullptr;
    zwlr_screencopy_manager_v1* manager = nullptr;
    std::vector<std::unique_ptr<OutputInfo>> outputs;
};

namespace {

// We only need the wl_shm path (guaranteed present at manager version <= 2),
// so binding at version 2 keeps the frame listener simple: no dmabuf/damage
// negotiation to handle.
constexpr uint32_t kScreencopyManagerVersion = 2;

// wl_output.name (used to pick a specific monitor) requires version 4.
constexpr uint32_t kOutputVersion = 4;

struct FrameState {
    uint32_t format = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    bool haveBufferInfo = false;
    bool yInverted = false;
    bool ready = false;
    bool failed = false;
};

void outputName(void* data, wl_output*, const char* name) {
    static_cast<OutputInfo*>(data)->name = name;
}
void outputGeometry(void*, wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t, const char*,
                     const char*, int32_t) {}
void outputMode(void*, wl_output*, uint32_t, int32_t, int32_t, int32_t) {}
void outputDone(void*, wl_output*) {}
void outputScale(void*, wl_output*, int32_t) {}
void outputDescription(void*, wl_output*, const char*) {}

constexpr wl_output_listener kOutputListener = {
    .geometry = outputGeometry,
    .mode = outputMode,
    .done = outputDone,
    .scale = outputScale,
    .name = outputName,
    .description = outputDescription,
};

void registryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface,
                     uint32_t version) {
    auto* self = static_cast<WaylandScreencopyCapture::Impl*>(data);
    if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        self->shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (std::strcmp(interface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        uint32_t bindVersion = std::min(version, kScreencopyManagerVersion);
        self->manager = static_cast<zwlr_screencopy_manager_v1*>(wl_registry_bind(
            registry, name, &zwlr_screencopy_manager_v1_interface, bindVersion));
    } else if (std::strcmp(interface, wl_output_interface.name) == 0) {
        uint32_t bindVersion = std::min(version, kOutputVersion);
        auto info = std::make_unique<OutputInfo>();
        info->output = static_cast<wl_output*>(
            wl_registry_bind(registry, name, &wl_output_interface, bindVersion));
        wl_output_add_listener(info->output, &kOutputListener, info.get());
        self->outputs.push_back(std::move(info));
    }
}

void registryGlobalRemove(void*, wl_registry*, uint32_t) {}

constexpr wl_registry_listener kRegistryListener = {
    .global = registryGlobal,
    .global_remove = registryGlobalRemove,
};

void frameBuffer(void* data, zwlr_screencopy_frame_v1*, uint32_t format, uint32_t width,
                  uint32_t height, uint32_t stride) {
    auto* state = static_cast<FrameState*>(data);
    state->format = format;
    state->width = width;
    state->height = height;
    state->stride = stride;
    state->haveBufferInfo = true;
}

void frameFlags(void* data, zwlr_screencopy_frame_v1*, uint32_t flags) {
    static_cast<FrameState*>(data)->yInverted =
        (flags & ZWLR_SCREENCOPY_FRAME_V1_FLAGS_Y_INVERT) != 0;
}

void frameReady(void* data, zwlr_screencopy_frame_v1*, uint32_t, uint32_t, uint32_t) {
    static_cast<FrameState*>(data)->ready = true;
}

void frameFailed(void* data, zwlr_screencopy_frame_v1*) {
    static_cast<FrameState*>(data)->failed = true;
}

void frameDamage(void*, zwlr_screencopy_frame_v1*, uint32_t, uint32_t, uint32_t, uint32_t) {}
void frameLinuxDmabuf(void*, zwlr_screencopy_frame_v1*, uint32_t, uint32_t, uint32_t) {}
void frameBufferDone(void*, zwlr_screencopy_frame_v1*) {}

constexpr zwlr_screencopy_frame_v1_listener kFrameListener = {
    .buffer = frameBuffer,
    .flags = frameFlags,
    .ready = frameReady,
    .failed = frameFailed,
    .damage = frameDamage,
    .linux_dmabuf = frameLinuxDmabuf,
    .buffer_done = frameBufferDone,
};

bool isSupportedShmFormat(uint32_t format) {
    return format == WL_SHM_FORMAT_ARGB8888 || format == WL_SHM_FORMAT_XRGB8888;
}

}  // namespace

WaylandScreencopyCapture::WaylandScreencopyCapture(const std::string& preferredOutputName)
    : impl_(std::make_unique<Impl>()) {
    impl_->display = wl_display_connect(nullptr);
    if (!impl_->display) {
        std::cerr << "[WaylandCapture] Error: could not connect to a Wayland display "
                     "(is WAYLAND_DISPLAY set?)\n";
        return;
    }

    impl_->registry = wl_display_get_registry(impl_->display);
    wl_registry_add_listener(impl_->registry, &kRegistryListener, impl_.get());
    wl_display_roundtrip(impl_->display);  // registry binds land here
    wl_display_roundtrip(impl_->display);  // per-output name/geometry/done events land here

    if (!impl_->shm) {
        std::cerr << "[WaylandCapture] Error: compositor did not advertise wl_shm\n";
    }
    if (!impl_->manager) {
        std::cerr << "[WaylandCapture] Error: compositor does not support "
                     "zwlr_screencopy_manager_v1 (wlroots-based compositors only, "
                     "e.g. Hyprland, Sway)\n";
    }

    if (impl_->outputs.empty()) {
        std::cerr << "[WaylandCapture] Error: compositor did not advertise any wl_output\n";
    } else if (!preferredOutputName.empty()) {
        for (auto& info : impl_->outputs) {
            if (info->name == preferredOutputName) {
                impl_->output = info->output;
                break;
            }
        }
        if (!impl_->output) {
            std::cerr << "[WaylandCapture] Error: no output named '" << preferredOutputName
                       << "'. Available: ";
            for (size_t i = 0; i < impl_->outputs.size(); ++i) {
                if (i) std::cerr << ", ";
                std::cerr << impl_->outputs[i]->name;
            }
            std::cerr << "\n";
        }
    } else {
        impl_->output = impl_->outputs.front()->output;
        std::cerr << "[WaylandCapture] Note: capture_output not set, auto-selecting '"
                   << impl_->outputs.front()->name
                   << "'. Set capture_output in settings.json to pick a specific monitor - "
                      "if the preview window overlaps the captured output, captures will "
                      "recursively include the window itself.\n";
    }
}

WaylandScreencopyCapture::~WaylandScreencopyCapture() {
    if (impl_->manager) zwlr_screencopy_manager_v1_destroy(impl_->manager);
    if (impl_->shm) wl_shm_destroy(impl_->shm);
    for (auto& info : impl_->outputs) wl_output_destroy(info->output);
    if (impl_->registry) wl_registry_destroy(impl_->registry);
    if (impl_->display) wl_display_disconnect(impl_->display);
}

bool WaylandScreencopyCapture::isSupported() const {
    return impl_->shm != nullptr && impl_->manager != nullptr && impl_->output != nullptr;
}

std::optional<CaptureFrame> WaylandScreencopyCapture::captureFrame() {
    if (!isSupported()) {
        return std::nullopt;
    }

    zwlr_screencopy_frame_v1* frame =
        zwlr_screencopy_manager_v1_capture_output(impl_->manager, /*overlay_cursor=*/1,
                                                   impl_->output);

    FrameState state;
    zwlr_screencopy_frame_v1_add_listener(frame, &kFrameListener, &state);

    // Wait for the "buffer" event describing the shm layout we must allocate.
    while (!state.haveBufferInfo && !state.failed) {
        if (wl_display_dispatch(impl_->display) < 0) {
            std::cerr << "[WaylandCapture] Error: Wayland dispatch failed while waiting for "
                         "buffer info\n";
            zwlr_screencopy_frame_v1_destroy(frame);
            return std::nullopt;
        }
    }
    if (state.failed) {
        std::cerr << "[WaylandCapture] Error: compositor reported capture failure\n";
        zwlr_screencopy_frame_v1_destroy(frame);
        return std::nullopt;
    }
    if (!isSupportedShmFormat(state.format)) {
        std::cerr << "[WaylandCapture] Error: unsupported shm buffer format 0x" << std::hex
                   << state.format << std::dec << " (only ARGB8888/XRGB8888 are supported)\n";
        zwlr_screencopy_frame_v1_destroy(frame);
        return std::nullopt;
    }

    const size_t size = static_cast<size_t>(state.stride) * state.height;
    int fd = memfd_create("luu-screencopy", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, static_cast<off_t>(size)) != 0) {
        std::cerr << "[WaylandCapture] Error: failed to create anonymous shm file: "
                   << std::strerror(errno) << "\n";
        if (fd >= 0) close(fd);
        zwlr_screencopy_frame_v1_destroy(frame);
        return std::nullopt;
    }

    void* map = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        std::cerr << "[WaylandCapture] Error: mmap failed: " << std::strerror(errno) << "\n";
        close(fd);
        zwlr_screencopy_frame_v1_destroy(frame);
        return std::nullopt;
    }

    wl_shm_pool* pool = wl_shm_create_pool(impl_->shm, fd, static_cast<int32_t>(size));
    wl_buffer* buffer = wl_shm_pool_create_buffer(
        pool, 0, static_cast<int32_t>(state.width), static_cast<int32_t>(state.height),
        static_cast<int32_t>(state.stride), state.format);
    wl_shm_pool_destroy(pool);  // buffer keeps the underlying memory alive

    zwlr_screencopy_frame_v1_copy(frame, buffer);

    while (!state.ready && !state.failed) {
        if (wl_display_dispatch(impl_->display) < 0) {
            std::cerr << "[WaylandCapture] Error: Wayland dispatch failed while waiting for "
                         "copy completion\n";
            munmap(map, size);
            close(fd);
            wl_buffer_destroy(buffer);
            zwlr_screencopy_frame_v1_destroy(frame);
            return std::nullopt;
        }
    }

    std::optional<CaptureFrame> result;
    if (state.ready) {
        CaptureFrame out;
        out.width = state.width;
        out.height = state.height;
        out.stride = state.stride;
        out.yInverted = state.yInverted;
        out.pixels.resize(size);
        std::memcpy(out.pixels.data(), map, size);
        result = std::move(out);
    } else {
        std::cerr << "[WaylandCapture] Error: compositor reported capture failure during copy\n";
    }

    munmap(map, size);
    close(fd);
    wl_buffer_destroy(buffer);
    zwlr_screencopy_frame_v1_destroy(frame);
    return result;
}

}  // namespace luu

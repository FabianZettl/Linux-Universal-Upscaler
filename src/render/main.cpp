// luu_capture_preview: continuous live-loop composition root.
//
// Every frame: grab a frame via wlr-screencopy, upload it as a GL texture,
// run it through the (placeholder) upscale shader, draw it. Paced by
// vsync (glfwSwapInterval(1)) rather than a hand-rolled rate limiter. A
// transient capture failure logs and keeps showing the last good frame
// instead of exiting - only the very first capture is fatal. Reads the
// same ~/.config/luu/settings.json the Python GUI writes, via luu::Config.
//
// Unoptimized on purpose for now: full re-capture + full shm alloc/copy +
// full texture re-upload every frame, no damage tracking / zero-copy
// DMA-BUF. That's deferred "Performance Optimization" work, not a gap.

#include <GL/glew.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "config.h"
#include "render_target.h"
#include "renderer.h"
#include "shader_program.h"
#include "texture.h"
#include "wayland_capture.h"

namespace {

std::filesystem::path defaultConfigPath() {
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : ".") / ".config" / "luu" / "settings.json";
}

void glfwErrorCallback(int code, const char* description) {
    std::cerr << "[GLFW] Error " << code << ": " << description << "\n";
}

}  // namespace

int main() {
    luu::Config config(defaultConfigPath());
    config.load();  // falls back to defaults on any error, already logged

    int targetWidth = 1920;
    int targetHeight = 1080;
    auto res = config.data().value("target_resolution", std::vector<int>{1920, 1080});
    if (res.size() == 2) {
        targetWidth = res[0];
        targetHeight = res[1];
    }
    std::string upscaleMode = config.get<std::string>("upscale_mode", "fsr");
    luu::FilterMode filter =
        (upscaleMode == "nearest") ? luu::FilterMode::Nearest : luu::FilterMode::Linear;
    bool useFsr = upscaleMode == "fsr";
    // RCAS sharpness is in "stops" - 0 is strongest. quality is otherwise
    // decorative for the other upscale modes.
    std::string quality = config.get<std::string>("quality", "high");
    float sharpness = 1.0f;
    if (quality == "low") sharpness = 2.0f;
    else if (quality == "medium") sharpness = 1.0f;
    else if (quality == "high") sharpness = 0.5f;
    else if (quality == "ultra") sharpness = 0.0f;
    std::string captureOutput = config.get<std::string>("capture_output", "");
    bool frameGenEnabled = config.get<bool>("frame_gen_enabled", true);
    std::string framegenMethod = config.get<std::string>("framegen_method", "lsfg");
    bool useFrameGen = frameGenEnabled && framegenMethod == "interpolation";
    if (frameGenEnabled && !useFrameGen) {
        std::cerr << "[luu_capture_preview] Note: framegen_method '" << framegenMethod
                   << "' is not implemented yet (only \"interpolation\" is) - frame "
                      "generation is off this run.\n";
    }

    glfwSetErrorCallback(glfwErrorCallback);
    // GLEW only understands GLX (no EGL/Wayland-native build on most distros),
    // so the render window goes through XWayland/GLX. Screencopy capture
    // itself uses its own, separate native Wayland connection (see
    // WaylandScreencopyCapture) and is unaffected by this choice.
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    if (!glfwInit()) {
        std::cerr << "[luu_capture_preview] Error: glfwInit failed (no X11/XWayland session?)\n";
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(targetWidth, targetHeight,
                                           "Linux Universal Upscaler - Capture Preview", nullptr,
                                           nullptr);
    if (!window) {
        std::cerr << "[luu_capture_preview] Error: failed to create GLFW window\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        std::cerr << "[luu_capture_preview] Error: glewInit failed: "
                   << glewGetErrorString(glewStatus) << "\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glGetError();  // clear the benign GL_INVALID_ENUM glewInit() leaves on core profiles
    glfwSwapInterval(1);  // paces the capture/upload/draw loop to the display refresh rate

    luu::WaylandScreencopyCapture capture(captureOutput);
    if (!capture.isSupported()) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    auto frame = capture.captureFrame();
    if (!frame) {
        std::cerr << "[luu_capture_preview] Error: capture failed\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // textures[current] is always "the latest real frame". With frame gen
    // on, textures[1-current] is "the one before that", used as the blend
    // source on in-between (non-capture) ticks.
    luu::Texture textures[2];
    int current = 0;
    int capturedCount = 1;
    textures[current].uploadBGRA(*frame, filter);

    // TODO: once there's an install step, load shaders from an installed
    // data dir instead of the source tree.
    std::string shaderDir = LUU_SHADER_DIR;
    luu::ShaderProgram program;
    if (!program.loadFromFiles(shaderDir + "/fullscreen.vert", shaderDir + "/upscale.frag")) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    luu::ShaderProgram blendProgram;
    if (useFrameGen &&
        !blendProgram.loadFromFiles(shaderDir + "/fullscreen.vert", shaderDir + "/framegen.frag")) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    luu::ShaderProgram easuProgram, rcasProgram;
    if (useFsr && (!easuProgram.loadFromFiles(shaderDir + "/fullscreen.vert",
                                               shaderDir + "/fsr_easu.frag") ||
                   !rcasProgram.loadFromFiles(shaderDir + "/fullscreen.vert",
                                               shaderDir + "/fsr_rcas.frag"))) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    luu::Renderer renderer;
    renderer.init();

    // easuTarget: EASU's upscaled output, read by RCAS. blendTarget: only
    // needed when frame gen AND fsr are both on, to materialize a blend
    // tick's crossfade into a texture EASU can read (frame-gen alone still
    // draws its blend straight to the screen, unchanged).
    std::optional<luu::RenderTarget> easuTarget;
    std::optional<luu::RenderTarget> blendTarget;
    if (useFsr) {
        easuTarget.emplace(targetWidth, targetHeight);
        if (useFrameGen) {
            blendTarget.emplace(static_cast<int>(frame->width), static_cast<int>(frame->height));
        }
    }

    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    std::cerr << "[luu_capture_preview] Captured " << frame->width << "x" << frame->height
               << " -> live preview at " << targetWidth << "x" << targetHeight
               << " (mode: " << upscaleMode << ", frame gen: " << (useFrameGen ? "on" : "off")
               << "). Esc or close the window to exit.\n";

    int framesThisSecond = 0;
    auto fpsWindowStart = std::chrono::steady_clock::now();
    long frameCounter = 0;

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // With frame gen on, only capture on even ticks; odd ticks reuse
        // the last two real captures via a crossfade instead of re-issuing
        // a screencopy request.
        bool isCaptureTick = !useFrameGen || (frameCounter % 2 == 0);
        if (isCaptureTick) {
            if (auto nextFrame = capture.captureFrame()) {
                current = 1 - current;
                textures[current].uploadBGRA(*nextFrame, filter);
                if (capturedCount < 2) ++capturedCount;
            } else {
                std::cerr << "[luu_capture_preview] Warning: capture failed this frame, "
                             "keeping last image\n";
            }
        }

        bool tickIsBlend = useFrameGen && !isCaptureTick && capturedCount >= 2;

        if (useFsr) {
            // Produce a capture-resolution source texture first (materializing
            // the blend into blendTarget on a frame-gen tick), then EASU
            // upscales it into easuTarget, then RCAS sharpens that onto the
            // screen.
            unsigned int sourceId;
            bool sourceFlip;
            if (tickIsBlend) {
                blendTarget->bind();
                renderer.drawBlend(blendProgram, textures[1 - current], textures[current]);
                sourceId = blendTarget->textureId();
                sourceFlip = false;  // framegen.frag already resolves flip on its inputs
            } else {
                sourceId = textures[current].id();
                sourceFlip = textures[current].flipY();
            }

            easuTarget->bind();
            renderer.drawFsrEasu(easuProgram, sourceId, sourceFlip,
                                  static_cast<float>(frame->width),
                                  static_cast<float>(frame->height),
                                  static_cast<float>(targetWidth),
                                  static_cast<float>(targetHeight));

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, fbWidth, fbHeight);
            glClear(GL_COLOR_BUFFER_BIT);
            renderer.drawFsrRcas(rcasProgram, easuTarget->textureId(), sharpness);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, fbWidth, fbHeight);
            glClear(GL_COLOR_BUFFER_BIT);
            if (tickIsBlend) {
                renderer.drawBlend(blendProgram, textures[1 - current], textures[current]);
            } else {
                renderer.drawFullscreen(program, textures[current]);
            }
        }
        glfwSwapBuffers(window);
        glfwPollEvents();

        ++frameCounter;
        ++framesThisSecond;
        auto now = std::chrono::steady_clock::now();
        if (now - fpsWindowStart >= std::chrono::seconds(1)) {
            std::cerr << "[luu_capture_preview] ~" << framesThisSecond << " fps\n";
            framesThisSecond = 0;
            fpsWindowStart = now;
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

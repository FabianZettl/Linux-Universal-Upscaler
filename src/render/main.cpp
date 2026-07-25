// luu_capture_preview: Phase 2 MVP composition root.
//
// Single-shot pipeline: grab one frame via wlr-screencopy, upload it as a
// GL texture, run it through the (placeholder) upscale shader, show the
// result in a window until closed / Esc is pressed. Reads the same
// ~/.config/luu/settings.json the Python GUI writes, via luu::Config.

#include <GL/glew.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "config.h"
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

    luu::WaylandScreencopyCapture capture;
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

    luu::Texture texture;
    texture.uploadBGRA(*frame, filter);

    // TODO: once there's an install step, load shaders from an installed
    // data dir instead of the source tree.
    luu::ShaderProgram program;
    std::string shaderDir = LUU_SHADER_DIR;
    if (!program.loadFromFiles(shaderDir + "/fullscreen.vert", shaderDir + "/upscale.frag")) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    luu::Renderer renderer;
    renderer.init();

    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    std::cerr << "[luu_capture_preview] Captured " << frame->width << "x" << frame->height
               << " -> displaying at " << targetWidth << "x" << targetHeight
               << " (mode: " << upscaleMode << "). Esc or close the window to exit.\n";

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        glClear(GL_COLOR_BUFFER_BIT);
        renderer.drawFullscreen(program, texture);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

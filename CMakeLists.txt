cmake_minimum_required(VERSION 3.20)
project(LinuxUniversalUpscaler LANGUAGES CXX C)  # C needed to compile wayland-scanner's generated protocol code

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
find_package(glfw3 3.3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)

find_package(Vulkan QUIET)
if(NOT TARGET Vulkan::Headers)
    include(FetchContent)
    FetchContent_Declare(
        VulkanHeaders
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(VulkanHeaders)
endif()

# Phase 1: core config parsing.
add_subdirectory(src/core)

# Phase 2: Wayland screencopy capture + GL upscale renderer (live loop +
# crossfade frame-gen MVP).
add_subdirectory(src/capture)
add_subdirectory(src/render)

# Vulkan layer skeleton: hooks vkQueuePresentKHR inside Vulkan apps
# directly (passthrough + logging only for now, see src/vulkan/hooks.cpp).
add_subdirectory(src/vulkan)

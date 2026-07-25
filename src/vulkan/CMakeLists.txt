add_library(VkLayer_luu_upscaler SHARED
    vk_layer.cpp
    hooks.cpp
)

target_link_libraries(VkLayer_luu_upscaler PRIVATE Vulkan::Headers)
set_target_properties(VkLayer_luu_upscaler PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    OUTPUT_NAME "VkLayer_luu_upscaler"
)

# Dev-mode manifest: generated into the build tree (not checked in) with an
# absolute path to the .so built alongside it, so VK_LAYER_PATH can just
# point at this directory without any install step. Matches CMake's default
# per-source-dir output location for this target.
set(LUU_VULKAN_LAYER_LIBRARY_PATH "${CMAKE_CURRENT_BINARY_DIR}/libVkLayer_luu_upscaler.so")
configure_file(
    layer/VK_LAYER_LinuxUniversalUpscaler.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/layer/VK_LAYER_LinuxUniversalUpscaler.json
    @ONLY
)

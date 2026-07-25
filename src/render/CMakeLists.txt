add_library(luu_render STATIC
    shader_program.cpp
    texture.cpp
    renderer.cpp
)
target_include_directories(luu_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(luu_render PUBLIC luu_capture GLEW::GLEW OpenGL::GL)

add_executable(luu_capture_preview main.cpp)
target_link_libraries(luu_capture_preview PRIVATE
    luu_render
    luu_capture
    luu_core
    glfw
    GLEW::GLEW
    OpenGL::GL
)
target_compile_definitions(luu_capture_preview PRIVATE
    LUU_SHADER_DIR="${CMAKE_SOURCE_DIR}/src/shaders"
)

find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)

set(LUU_PROTOCOL_DIR ${CMAKE_SOURCE_DIR}/protocols)
set(LUU_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${LUU_GENERATED_DIR})

set(SCREENCOPY_XML ${LUU_PROTOCOL_DIR}/wlr-screencopy-unstable-v1.xml)
set(SCREENCOPY_HEADER ${LUU_GENERATED_DIR}/wlr-screencopy-unstable-v1-client-protocol.h)
set(SCREENCOPY_SOURCE ${LUU_GENERATED_DIR}/wlr-screencopy-unstable-v1-client-protocol.c)

add_custom_command(
    OUTPUT ${SCREENCOPY_HEADER}
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${SCREENCOPY_XML} ${SCREENCOPY_HEADER}
    DEPENDS ${SCREENCOPY_XML}
    COMMENT "Generating wlr-screencopy client header"
)
add_custom_command(
    OUTPUT ${SCREENCOPY_SOURCE}
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code ${SCREENCOPY_XML} ${SCREENCOPY_SOURCE}
    DEPENDS ${SCREENCOPY_XML}
    COMMENT "Generating wlr-screencopy protocol marshalling code"
)

add_library(luu_capture STATIC
    wayland_capture.cpp
    ${SCREENCOPY_SOURCE}
    ${SCREENCOPY_HEADER}
)

target_include_directories(luu_capture
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${LUU_GENERATED_DIR} ${WAYLAND_CLIENT_INCLUDE_DIRS}
)
target_link_directories(luu_capture PRIVATE ${WAYLAND_CLIENT_LIBRARY_DIRS})
target_link_libraries(luu_capture PUBLIC ${WAYLAND_CLIENT_LIBRARIES})

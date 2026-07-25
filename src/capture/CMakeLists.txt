find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)
pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)

set(LUU_PROTOCOL_DIR ${CMAKE_SOURCE_DIR}/protocols)
set(LUU_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${LUU_GENERATED_DIR})

# Generates client-header + private-code for a protocol XML into
# LUU_GENERATED_DIR, appending both to LUU_PROTOCOL_SOURCES (parent scope).
function(luu_generate_wayland_protocol xml_path base_name)
    set(header ${LUU_GENERATED_DIR}/${base_name}-client-protocol.h)
    set(source ${LUU_GENERATED_DIR}/${base_name}-client-protocol.c)
    add_custom_command(
        OUTPUT ${header}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${xml_path} ${header}
        DEPENDS ${xml_path}
        COMMENT "Generating ${base_name} client header"
    )
    add_custom_command(
        OUTPUT ${source}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code ${xml_path} ${source}
        DEPENDS ${xml_path}
        COMMENT "Generating ${base_name} protocol marshalling code"
    )
    set(LUU_PROTOCOL_SOURCES ${LUU_PROTOCOL_SOURCES} ${header} ${source} PARENT_SCOPE)
endfunction()

# Vendored (see protocols/README context: wlr-screencopy isn't part of the
# official wayland-protocols package).
luu_generate_wayland_protocol(${LUU_PROTOCOL_DIR}/wlr-screencopy-unstable-v1.xml
                               wlr-screencopy-unstable-v1)

# Part of the system wayland-protocols package (1.49+) - no vendoring needed.
luu_generate_wayland_protocol(
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml
    ext-foreign-toplevel-list-v1)
luu_generate_wayland_protocol(
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-capture-source/ext-image-capture-source-v1.xml
    ext-image-capture-source-v1)
luu_generate_wayland_protocol(
    ${WAYLAND_PROTOCOLS_DATADIR}/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml
    ext-image-copy-capture-v1)

add_library(luu_capture STATIC
    wayland_capture.cpp
    wayland_toplevel_capture.cpp
    ${LUU_PROTOCOL_SOURCES}
)

target_include_directories(luu_capture
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${LUU_GENERATED_DIR} ${WAYLAND_CLIENT_INCLUDE_DIRS}
)
target_link_directories(luu_capture PRIVATE ${WAYLAND_CLIENT_LIBRARY_DIRS})
target_link_libraries(luu_capture PUBLIC ${WAYLAND_CLIENT_LIBRARIES})

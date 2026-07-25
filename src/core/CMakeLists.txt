add_library(luu_core STATIC
    config.cpp
)

target_include_directories(luu_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(luu_core PUBLIC nlohmann_json::nlohmann_json)

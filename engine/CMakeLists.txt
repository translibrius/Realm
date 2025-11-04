# Engine library
add_library(Engine SHARED
    src/platform/platform_win32.c
)

target_include_directories(Engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
)

# Define macro for export/import declspec
target_compile_definitions(Engine PRIVATE ENGINE_BUILD)
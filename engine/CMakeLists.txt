# Engine library
add_library(Engine SHARED
    src/platform/platform_win32.c
    src/memory/arena.c
    src/memory/memory.c
    src/util/assert.c
)

target_include_directories(Engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/src/memory
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/util
)

# Define macro for export/import declspec
target_compile_definitions(Engine PRIVATE ENGINE_BUILD)
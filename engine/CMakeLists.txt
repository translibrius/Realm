# Engine library
# Collect all .c and .h files automatically
file(GLOB_RECURSE ENGINE_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(Engine SHARED ${ENGINE_SOURCES})

target_include_directories(Engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Define macro for export/import declspec
target_compile_definitions(Engine PRIVATE ENGINE_BUILD)
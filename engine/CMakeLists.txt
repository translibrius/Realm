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

if (WIN32)
    target_link_libraries(Engine PRIVATE opengl32)
elseif (UNIX)
    find_package(OpenGL REQUIRED)
    target_link_libraries(Engine PRIVATE OpenGL::GL)
endif()

# Define macro for export/import declspec
target_compile_definitions(Engine PRIVATE ENGINE_BUILD)
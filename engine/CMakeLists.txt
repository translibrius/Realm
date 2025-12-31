# Engine library

# --- Vendor (C++) ---
# Add vendor first so targets exist
set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "" FORCE)
# Force MSDFGEN to use dynamic runtime (/MD, /MDd)
set(MSDFGEN_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)
set(MSDF_ATLAS_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)
# Make sure vcpkg uses dynamic CRT too
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "" FORCE)
add_subdirectory(src/vendor/msdf-atlas-gen)

# --- C sources ---
file(GLOB_RECURSE ENGINE_C_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

# Build C part as a static lib (no C++ here)
add_library(EngineC STATIC ${ENGINE_C_SOURCES})

target_include_directories(EngineC
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Warnings only for my own code (no vendor)
target_compile_options(EngineC PRIVATE
        -Wall -Wextra -Wshadow -Wpedantic -Werror
        -Wno-newline-eof
        -Wno-missing-braces
        -Wno-gnu-zero-variadic-macro-arguments
)

# Per-configuration compiler flags
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(EngineC PRIVATE -O0 -g3 -fno-omit-frame-pointer)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(EngineC PRIVATE -O3 -DNDEBUG -march=native -fno-omit-frame-pointer)
endif ()

if (WIN32)
    target_link_libraries(EngineC PRIVATE opengl32)
elseif (UNIX)
    find_package(OpenGL REQUIRED)
    target_link_libraries(EngineC PRIVATE OpenGL::GL)
endif ()

# --- C++ sources ---
add_library(EngineCPP STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/font/msdf_wrapper.cpp
)

# Give the wrapper access to C engine headers
target_include_directories(EngineCPP
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/msdf-atlas-gen
)

# IMPORTANT: do NOT use strict warnings here (vendor headers will trigger them)
target_compile_options(EngineCPP PRIVATE
        -Wno-error
        -Wno-shadow
        -Wno-unused-parameter
        -Wno-pedantic
        -Wno-int-in-bool-context
)

# Ensure C++ standard
target_compile_features(EngineCPP PRIVATE cxx_std_17)
target_link_libraries(EngineCPP PRIVATE msdf-atlas-gen)
target_link_libraries(EngineCPP PRIVATE EngineC)

# --- Final shared library that your app links against ---
add_library(Engine INTERFACE)

target_link_libraries(Engine
        INTERFACE
        EngineC
        EngineCPP
)

# Optional: export include dir from Engine too (so Realm can include engine headers)
target_include_directories(Engine
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

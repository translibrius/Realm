# Engine library

#set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic")

# --- Vendor (C++) ---
# Add vendor first so targets exist
set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)
set(MSDF_ATLAS_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)
# Make sure vcpkg uses dynamic CRT too
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "" FORCE)
add_subdirectory(vendor/msdf-atlas-gen)

# Tracy options
set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
set(TRACY_ON_DEMAND OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/tracy)

# Vulkan
find_package(Vulkan REQUIRED)


# --- C sources ---as
file(GLOB_RECURSE ENGINE_C_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad/*.c"
)

# Build C part as an object library (no C++ here)
add_library(EngineC OBJECT ${ENGINE_C_SOURCES})

target_compile_definitions(EngineC PRIVATE
        VK_NO_PROTOTYPES
)

set(SHADERC_ROOT "$ENV{VULKAN_SDK}")
target_include_directories(EngineC PRIVATE
        ${SHADERC_ROOT}/Include
)


target_include_directories(EngineC
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src

        # Vendor
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cglm
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/volk
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/yyjson
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/clay
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/msdf-atlas-gen/msdf-atlas-gen
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tracy/public/tracy
        ${Vulkan_INCLUDE_DIRS} # For shaders
)

# Warnings only for my own code (no vendor)
target_compile_options(EngineC PRIVATE
        -Wall -Wextra -Wshadow
        -Wno-newline-eof
        -Wno-missing-braces
        -Wno-gnu-zero-variadic-macro-arguments
)


# --- C++ sources ---
add_library(EngineCPP OBJECT
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/font/msdf_wrapper.cpp
)

# Give the wrapper access to C engine headers
target_include_directories(EngineCPP
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        vendor/msdf-atlas-gen
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

# Per-configuration compiler flags
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(EngineC PRIVATE -O0 -g3 -fno-omit-frame-pointer)
    target_compile_options(EngineCPP PRIVATE -O0 -g3 -fno-omit-frame-pointer)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(EngineC PRIVATE -O3 -DNDEBUG -march=native -fno-omit-frame-pointer)
    target_compile_options(EngineCPP PRIVATE -O3 -DNDEBUG -march=native -fno-omit-frame-pointer)
endif ()

# --- Final shared library that your app links against ---
add_library(Engine SHARED
        $<TARGET_OBJECTS:EngineC>
        $<TARGET_OBJECTS:EngineCPP>
)

target_link_libraries(Engine PRIVATE
        Vulkan::Headers
        ${SHADERC_ROOT}/Lib/shaderc_shared.lib
        Tracy::TracyClient
        msdf-atlas-gen
)

# OpenGL link
if (WIN32)
    target_link_libraries(Engine PRIVATE opengl32)
elseif (UNIX)
    find_package(OpenGL REQUIRED)
    target_link_libraries(Engine PRIVATE OpenGL::GL)
endif ()

target_compile_definitions(EngineC PRIVATE ENGINE_SHARED ENGINE_BUILD)
target_compile_definitions(EngineCPP PRIVATE ENGINE_SHARED ENGINE_BUILD)

target_compile_definitions(Engine INTERFACE ENGINE_SHARED)

# Export include dirs from Engine (so Realm and modules include engine headers)
target_include_directories(Engine
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cglm
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tracy/public/tracy
)

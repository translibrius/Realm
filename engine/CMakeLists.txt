# Engine library

#set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic")

# --- Vendor (C++) ---
# Add vendor first so targets exist
set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)
set(MSDF_ATLAS_DYNAMIC_RUNTIME ON CACHE BOOL "" FORCE)
# Make sure vcpkg uses dynamic CRT too (Windows-only)
if (WIN32)
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "" FORCE)
endif ()
add_subdirectory(vendor/msdf-atlas-gen)


function(rl_silence_third_party_warnings target)
    if (TARGET ${target})
        if (MSVC)
            target_compile_options(${target} PRIVATE /W0)
        else ()
            target_compile_options(${target} PRIVATE -w)
        endif ()
        if (WIN32)
            target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif ()
    endif ()
endfunction()

rl_silence_third_party_warnings(msdfgen-core)
rl_silence_third_party_warnings(msdfgen-ext)
rl_silence_third_party_warnings(msdf-atlas-gen)

# Vulkan
find_package(Vulkan REQUIRED)

# Objective-C (macOS platform layer)
if (APPLE)
    enable_language(OBJC)
endif ()

# --- C sources ---as
file(GLOB_RECURSE ENGINE_C_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad/*.c"
)

if (APPLE)
    file(GLOB_RECURSE ENGINE_OBJC_SOURCES
            CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.m"
    )
    list(APPEND ENGINE_C_SOURCES ${ENGINE_OBJC_SOURCES})
endif ()

if (NOT WIN32)
    list(FILTER ENGINE_C_SOURCES EXCLUDE REGEX "glad_wgl\\.c$")
endif ()

# Build C part as an object library (no C++ here)
add_library(EngineC OBJECT ${ENGINE_C_SOURCES})

target_compile_definitions(EngineC PRIVATE
        VK_NO_PROTOTYPES
)

# Shaderc: prefer Vulkan SDK shared lib on Windows (local dev); fall back to vcpkg.
if (WIN32 AND DEFINED ENV{VULKAN_SDK} AND EXISTS "$ENV{VULKAN_SDK}/Lib/shaderc_shared.lib")
    set(SHADERC_ROOT "$ENV{VULKAN_SDK}")
    target_include_directories(EngineC PRIVATE "${SHADERC_ROOT}/Include")
    set(SHADERC_TARGET "${SHADERC_ROOT}/Lib/shaderc_shared.lib")
else ()
    find_package(unofficial-shaderc CONFIG REQUIRED)
    set(SHADERC_TARGET unofficial::shaderc::shaderc)
endif ()

target_link_libraries(EngineC PRIVATE ${SHADERC_TARGET})


target_include_directories(EngineC
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Vendor headers as SYSTEM to suppress their warnings
target_include_directories(EngineC SYSTEM
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cglm
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/volk
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/yyjson
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/clay
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cgltf
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/msdf-atlas-gen/msdf-atlas-gen
        ${Vulkan_INCLUDE_DIRS}
)

# Warnings only for my own code (no vendor)
target_compile_options(EngineC PRIVATE
        -Wall -Wextra -Wshadow
        -Wno-newline-eof
        -Wno-missing-braces
        -Wno-gnu-zero-variadic-macro-arguments
        $<$<BOOL:${REALM_WERROR}>:-Werror>
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

rl_set_compiler_flags(EngineC)
rl_set_compiler_flags(EngineCPP)

# --- Final shared library that your app links against ---
add_library(Engine SHARED
        $<TARGET_OBJECTS:EngineC>
        $<TARGET_OBJECTS:EngineCPP>
)

target_link_libraries(Engine PRIVATE
        Vulkan::Headers
        ${SHADERC_TARGET}
        msdf-atlas-gen
)

if (APPLE)
    target_link_libraries(Engine PRIVATE "-framework Cocoa" "-framework QuartzCore" "-framework Metal")
endif ()

if (UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(Engine PRIVATE X11::X11 pthread dl)
endif ()

# OpenGL link
if (WIN32)
    target_link_libraries(Engine PRIVATE opengl32)
elseif (UNIX)
    find_package(OpenGL REQUIRED)
    target_link_libraries(Engine PRIVATE OpenGL::GL)
endif ()

target_compile_definitions(EngineC PRIVATE ENGINE_SHARED ENGINE_BUILD
        REALM_VERSION="${PROJECT_VERSION}")
if (WIN32)
    target_compile_definitions(EngineC PRIVATE CLAY_DLL)
elseif (UNIX AND NOT APPLE)
    target_compile_definitions(EngineC PRIVATE _DEFAULT_SOURCE)
endif ()
target_compile_definitions(EngineCPP PRIVATE ENGINE_SHARED ENGINE_BUILD)

target_compile_definitions(Engine INTERFACE ENGINE_SHARED)

# --- Profiling ---
if (RL_PROFILE)
    target_compile_options(EngineC PRIVATE -finstrument-functions)
    target_compile_definitions(EngineC PRIVATE RL_PROFILE_ENABLED=1)
    target_compile_definitions(Engine INTERFACE RL_PROFILE_ENABLED=1)
endif ()

# Export include dirs from Engine (so Realm and modules include engine headers)
target_include_directories(Engine
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(Engine SYSTEM
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/cglm
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/clay
)

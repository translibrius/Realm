cmake_minimum_required(VERSION 3.20)

project(Realm VERSION 0.0.1 DESCRIPTION "3D Engine - Realm" LANGUAGES C)

# C Standart
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Common warnings for all builds
add_compile_options(-Wall -Werror -Wextra -Wshadow -Wpedantic -Wno-newline-eof)

# Per-configuration compiler flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Configuring Debug build no optimizations")
    add_compile_options(-O0 -g3 -fno-omit-frame-pointer)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Configuring Release build with optimizations")
    add_compile_options(-O3 -DNDEBUG -march=native -fno-omit-frame-pointer)
endif()

# Set unified output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)   # .lib, .a
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)   # .dll, .so, .dylib
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)   # .exe

# Add subprojects
add_subdirectory(engine)
add_subdirectory(realm)
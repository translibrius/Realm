file(GLOB_RECURSE REALM_C_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_executable(Realm
        ${REALM_C_SOURCES}
)

# Per-configuration compiler flags
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(Realm PRIVATE -O0 -g3 -fno-omit-frame-pointer)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(Realm PRIVATE -O3 -DNDEBUG -march=native -fno-omit-frame-pointer)
endif ()

# Engine last
target_link_libraries(Realm PRIVATE Engine)

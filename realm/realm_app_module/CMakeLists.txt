add_library(realm_app SHARED
        src/realm_app_stub.c
        src/game.c
)

target_include_directories(realm_app
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(realm_app PRIVATE REALM_APP_BUILD=1 ENGINE_SHARED)

# Per-configuration compiler flags
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(realm_app PRIVATE -O0 -g3 -fno-omit-frame-pointer)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(realm_app PRIVATE -O3 -DNDEBUG -march=native -fno-omit-frame-pointer)
endif ()

target_link_libraries(realm_app PRIVATE Engine)

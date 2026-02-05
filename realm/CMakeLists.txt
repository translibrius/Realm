file(GLOB_RECURSE REALM_C_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_executable(Realm
        ${REALM_C_SOURCES}
)

target_include_directories(Realm
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Per-configuration compiler flags
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(Realm PRIVATE -O0 -g3 -fno-omit-frame-pointer)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(Realm PRIVATE -O3 -DNDEBUG -march=native -fno-omit-frame-pointer)
endif ()

# Engine last
target_link_libraries(Realm PRIVATE Engine Tracy::TracyClient)
target_compile_definitions(Realm PRIVATE ENGINE_SHARED)

if (WIN32)
    target_compile_definitions(Realm PRIVATE
            REALM_APP_MODULE_NAME="realm_app.dll"
            REALM_APP_HOT_FORMAT="realm_app_hot_%u.dll"
            REALM_APP_HAS_PDB=1
            REALM_APP_PDB_NAME="realm_app.pdb"
            REALM_APP_HOT_PDB_FORMAT="realm_app_hot_%u.pdb"
    )
elseif (APPLE)
    target_compile_definitions(Realm PRIVATE
            REALM_APP_MODULE_NAME="librealm_app.dylib"
            REALM_APP_HOT_FORMAT="librealm_app_hot_%u.dylib"
            REALM_APP_HAS_PDB=0
    )
elseif (UNIX)
    target_compile_definitions(Realm PRIVATE
            REALM_APP_MODULE_NAME="librealm_app.so"
            REALM_APP_HOT_FORMAT="librealm_app_hot_%u.so"
            REALM_APP_HAS_PDB=0
    )
endif ()

add_subdirectory(realm_app_module)

add_dependencies(Realm realm_app)

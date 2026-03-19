file(GLOB_RECURSE REALM_C_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Windows application icon
if (WIN32)
    set(REALM_RC "${CMAKE_CURRENT_SOURCE_DIR}/realm.rc")
endif ()

# macOS application icon
if (APPLE)
    set(REALM_ICNS "${CMAKE_SOURCE_DIR}/assets/icons/realm.icns")
endif ()

add_executable(Realm
        ${REALM_C_SOURCES}
        ${REALM_RC}
        ${REALM_ICNS}
)

target_include_directories(Realm
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

rl_set_compiler_flags(Realm)

if (REALM_WERROR)
    target_compile_options(Realm PRIVATE -Werror)
endif ()

# Engine contains C++ objects; use C++ linker so sanitizer runtimes resolve.
set_target_properties(Realm PROPERTIES LINKER_LANGUAGE CXX)

if (APPLE)
    set_target_properties(Realm PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE realm.icns
        MACOSX_BUNDLE_BUNDLE_NAME "Realm"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.realm.game"
    )
    set_source_files_properties(${REALM_ICNS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
endif ()

# Engine last
target_link_libraries(Realm PRIVATE Engine)
target_compile_definitions(Realm PRIVATE ENGINE_SHARED)
target_compile_definitions(Realm PRIVATE REALM_APP_BUILD_DIR="${CMAKE_BINARY_DIR}")
target_compile_definitions(Realm PRIVATE REALM_APP_BUILD_TOOL="${CMAKE_COMMAND}")

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

if (RL_PROFILE)
    target_compile_options(Realm PRIVATE -finstrument-functions)
    target_compile_definitions(Realm PRIVATE REALM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

    # Copy profiler scripts alongside Realm binary
    add_custom_command(TARGET Realm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/profiler_view.py"
            "$<TARGET_FILE_DIR:Realm>/profiler_view.py"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../tools/profiler_report.py"
            "$<TARGET_FILE_DIR:Realm>/profiler_report.py"
        COMMENT "Copying profiler scripts to output directory"
    )
endif ()

add_subdirectory(realm_app_module)

add_dependencies(Realm realm_app)

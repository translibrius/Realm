file(GLOB_RECURSE EDITOR_C_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Windows application icon
if (WIN32)
    set(EDITOR_RC "${CMAKE_CURRENT_SOURCE_DIR}/realm_editor.rc")
endif ()

# macOS application icon
if (APPLE)
    set(EDITOR_ICNS "${CMAKE_SOURCE_DIR}/assets/icons/realm.icns")
endif ()

add_executable(RealmEditor
        ${EDITOR_C_SOURCES}
        ${EDITOR_RC}
        ${EDITOR_ICNS}
)

target_include_directories(RealmEditor
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

rl_set_compiler_flags(RealmEditor)

if (REALM_WERROR)
    target_compile_options(RealmEditor PRIVATE -Werror)
endif ()

# Engine contains C++ objects; use C++ linker so sanitizer runtimes resolve.
set_target_properties(RealmEditor PROPERTIES LINKER_LANGUAGE CXX)

if (APPLE)
    set_target_properties(RealmEditor PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE realm.icns
        MACOSX_BUNDLE_BUNDLE_NAME "Realm Editor"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.realm.editor"
    )
    set_source_files_properties(${EDITOR_ICNS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
endif ()

target_link_libraries(RealmEditor PRIVATE Engine)
target_compile_definitions(RealmEditor PRIVATE ENGINE_SHARED)

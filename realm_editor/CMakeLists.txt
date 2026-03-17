file(GLOB_RECURSE EDITOR_C_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Windows application icon
if (WIN32)
    set(EDITOR_RC "${CMAKE_CURRENT_SOURCE_DIR}/realm_editor.rc")
endif ()

add_executable(RealmEditor
        ${EDITOR_C_SOURCES}
        ${EDITOR_RC}
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

target_link_libraries(RealmEditor PRIVATE Engine)
target_compile_definitions(RealmEditor PRIVATE ENGINE_SHARED)

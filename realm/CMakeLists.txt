# --- C sources ---as
file(GLOB_RECURSE REALM_C_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# Realm executable
add_executable(Realm
        ${REALM_C_SOURCES}
)

# Link the Engine library to Realm
target_link_libraries(Realm PRIVATE Engine)

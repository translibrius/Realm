file(GLOB_RECURSE REALM_C_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_executable(Realm
        ${REALM_C_SOURCES}
)

# Engine last
target_link_libraries(Realm PRIVATE Engine)

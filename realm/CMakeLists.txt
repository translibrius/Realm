# Realm executable
add_executable(Realm
    src/main.c
)

# Link the Engine library to Realm
target_link_libraries(Realm PRIVATE Engine)

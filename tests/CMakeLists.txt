file(GLOB TEST_CASES cases/*.c)

add_executable(RealmTests
        main.c
        harness/rl_test.c
        support/test_runtime.c
        ${TEST_CASES}
)

target_include_directories(RealmTests
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/harness
        ${CMAKE_CURRENT_SOURCE_DIR}/support
        ${CMAKE_SOURCE_DIR}/engine/src/gui
)

target_link_libraries(RealmTests PRIVATE Engine)
target_compile_definitions(RealmTests PRIVATE ENGINE_SHARED)

# Engine contains C++ objects (msdf-atlas-gen). On Linux with sanitizers, the
# C++ UBSan runtime must be linked into the executable, which requires the C++
# linker driver.
set_target_properties(RealmTests PROPERTIES LINKER_LANGUAGE CXX)

rl_set_compiler_flags(RealmTests)

add_test(NAME realm.unit COMMAND RealmTests --fail-fast)
set_tests_properties(realm.unit PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

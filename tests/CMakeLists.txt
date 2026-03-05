add_executable(RealmTests
        main.c
        harness/rl_test.c
        support/test_runtime.c
        cases/test_camera.c
        cases/test_config.c
        cases/test_str.c
        cases/test_memory.c
        cases/test_arena.c
        cases/test_dynamic_array.c
        cases/test_event.c
        cases/test_input.c
        cases/test_logger.c
        cases/test_file_io.c
        cases/test_clock.c
        cases/test_rand.c
        cases/test_gui_id.c
        cases/test_text_input.c
        cases/test_asset.c
        cases/test_profiler.c
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

# Engine contains C++ objects (msdf-atlas-gen, Tracy). On Linux with sanitizers,
# the C++ UBSan runtime must be linked into the executable, which requires the
# C++ linker driver.
set_target_properties(RealmTests PROPERTIES LINKER_LANGUAGE CXX)

rl_set_compiler_flags(RealmTests)

add_test(NAME realm.unit COMMAND RealmTests --fail-fast)
set_tests_properties(realm.unit PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_executable(RealmTests
        main.c
        harness/rl_test.c
        support/test_runtime.c
        cases/test_camera.c
        cases/test_config.c
        cases/test_str.c
)

target_include_directories(RealmTests
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/harness
        ${CMAKE_CURRENT_SOURCE_DIR}/support
)

target_link_libraries(RealmTests PRIVATE Engine)
target_compile_definitions(RealmTests PRIVATE ENGINE_SHARED)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(RealmTests PRIVATE -O0 -g3 -fno-omit-frame-pointer)
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(RealmTests PRIVATE -O3 -DNDEBUG -march=native -fno-omit-frame-pointer)
endif ()

add_test(NAME realm.unit COMMAND RealmTests --fail-fast)
set_tests_properties(realm.unit PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

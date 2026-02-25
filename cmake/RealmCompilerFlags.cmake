include_guard()

option(REALM_MARCH_NATIVE "Optimize for local CPU (-march=native)" ON)

function(rl_set_compiler_flags target)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${target} PRIVATE -O0 -g3 -fno-omit-frame-pointer)
    elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(${target} PRIVATE
            -O3 -DNDEBUG -fno-omit-frame-pointer
            $<$<BOOL:${REALM_MARCH_NATIVE}>:-march=native>
        )
    endif ()
endfunction()

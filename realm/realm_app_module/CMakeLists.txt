add_library(realm_app SHARED
        src/realm_app_api.c
        src/game.c
        src/scene_main_menu.c
        src/scene_game.c
        src/menu_pause.c
        src/menu_settings.c
)

target_include_directories(realm_app
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(realm_app PRIVATE REALM_APP_BUILD=1 ENGINE_SHARED)

rl_set_compiler_flags(realm_app)

if (REALM_WERROR)
    target_compile_options(realm_app PRIVATE -Werror)
endif ()

target_link_libraries(realm_app PRIVATE Engine)

if (RL_PROFILE)
    target_compile_options(realm_app PRIVATE -finstrument-functions)
endif ()

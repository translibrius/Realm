#include "core/project_template.h"

#include "core/logger.h"
#include "platform/io/file_io.h"
#include "util/str.h"

static b8 write_default_scene(const char *root) {
    char path[512];
    cstr_format_buf(path, sizeof(path), "%sscenes/default.scene", root);

    static const char content[] =
        "{\n"
        "  \"name\": \"Default Scene\",\n"
        "  \"entities\": [\n"
        "    {\n"
        "      \"name\": \"Light\",\n"
        "      \"transform\": {\n"
        "        \"position\": [1.2, 1.0, 2.0],\n"
        "        \"rotation\": [0.0, 0.0, 0.0],\n"
        "        \"scale\": [1.0, 1.0, 1.0]\n"
        "      },\n"
        "      \"light\": {\n"
        "        \"ambient\": [0.2, 0.2, 0.2],\n"
        "        \"diffuse\": [0.5, 0.5, 0.5],\n"
        "        \"specular\": [1.0, 1.0, 1.0]\n"
        "      }\n"
        "    },\n"
        "    {\n"
        "      \"name\": \"Camera\",\n"
        "      \"transform\": {\n"
        "        \"position\": [0.0, 0.0, 3.0],\n"
        "        \"rotation\": [0.0, -90.0, 0.0],\n"
        "        \"scale\": [1.0, 1.0, 1.0]\n"
        "      },\n"
        "      \"camera\": {\n"
        "        \"fov\": 90.0,\n"
        "        \"near\": 0.1,\n"
        "        \"far\": 100.0,\n"
        "        \"main\": true\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";

    return platform_file_write_all(path, content, cstr_len(content));
}

static b8 write_game_h(const char *root) {
    char path[512];
    cstr_format_buf(path, sizeof(path), "%ssrc/game.h", root);

    static const char content[] =
        "#pragma once\n"
        "\n"
        "#include <realm_app_api.h>\n"
        "#include \"core/camera.h\"\n"
        "#include \"memory/arena.h\"\n"
        "\n"
        "#define RL_GAME_STATE_VERSION 1\n"
        "\n"
        "typedef struct rl_game {\n"
        "    u32 version;\n"
        "    rl_camera camera;\n"
        "    rl_arena frame_arena;\n"
        "    const realm_app_context *app_context;\n"
        "} rl_game;\n"
        "\n"
        "b8   game_init(rl_game *game, const realm_app_context *ctx);\n"
        "void game_update(rl_game *game, const realm_app_context *ctx, realm_app_cmd_queue *cmds, f64 dt);\n"
        "void game_render(rl_game *game, const realm_app_context *ctx, realm_app_cmd_queue *cmds);\n"
        "void game_destroy(rl_game *game);\n";

    return platform_file_write_all(path, content, cstr_len(content));
}

static b8 write_game_c(const char *root) {
    char path[512];
    cstr_format_buf(path, sizeof(path), "%ssrc/game.c", root);

    static const char content[] =
        "#include \"game.h\"\n"
        "\n"
        "#include \"core/behavior.h\"\n"
        "#include \"core/camera.h\"\n"
        "#include \"core/component.h\"\n"
        "#include \"core/logger.h\"\n"
        "#include \"core/scene.h\"\n"
        "#include \"memory/memory.h\"\n"
        "#include \"renderer/renderer_frontend.h\"\n"
        "\n"
        "static void rotate_update(rl_scene *scene, rl_entity entity, f32 dt) {\n"
        "    rl_transform *t = transform_get(&scene->components, entity);\n"
        "    if (!t) return;\n"
        "    t->rotation[1] += 100.0f * dt;\n"
        "    if (t->rotation[1] > 360.0f) t->rotation[1] -= 360.0f;\n"
        "    t->dirty = true;\n"
        "}\n"
        "\n"
        "b8 game_init(rl_game *game, const realm_app_context *ctx) {\n"
        "    if (!game) return false;\n"
        "\n"
        "    b8 state_compatible = game->version == RL_GAME_STATE_VERSION;\n"
        "    if (!state_compatible) {\n"
        "        game->version = RL_GAME_STATE_VERSION;\n"
        "        camera_init(&game->camera);\n"
        "    }\n"
        "\n"
        "    // Init camera from scene entity if available\n"
        "    if (ctx->scene) {\n"
        "        rl_entity cam_e = scene_get_main_camera(ctx->scene);\n"
        "        if (cam_e != RL_ENTITY_INVALID) {\n"
        "            rl_transform *ct = transform_get(&ctx->scene->components, cam_e);\n"
        "            rl_camera_component *cc = camera_comp_get(&ctx->scene->components, cam_e);\n"
        "            if (ct && cc) camera_from_entity(&game->camera, ct, cc);\n"
        "        }\n"
        "    }\n"
        "    game->camera.look_speed = ctx->mouse_sensitivity;\n"
        "    game->app_context = ctx;\n"
        "\n"
        "    rl_arena_init(&game->frame_arena, KiB(512), KiB(64), MEM_ARENA);\n"
        "\n"
        "    behavior_registry_clear();\n"
        "    behavior_register(\"rotate\", rotate_update);\n"
        "\n"
        "    return true;\n"
        "}\n"
        "\n"
        "void game_update(rl_game *game, const realm_app_context *ctx, realm_app_cmd_queue *cmds, f64 dt) {\n"
        "    if (!game) return;\n"
        "\n"
        "    game->camera.fov = ctx->fov;\n"
        "    game->camera.look_speed = ctx->mouse_sensitivity;\n"
        "    realm_app_cmd_push(cmds, (realm_app_cmd){.type = REALM_APP_CMD_SET_CURSOR_VISIBLE, .b = false});\n"
        "\n"
        "    if (ctx->scene) {\n"
        "        behavior_update_all(ctx->scene, (f32)dt);\n"
        "    }\n"
        "\n"
        "    camera_update(&game->camera, dt);\n"
        "}\n"
        "\n"
        "void game_render(rl_game *game, const realm_app_context *ctx, realm_app_cmd_queue *cmds) {\n"
        "    if (!game || !ctx->scene) return;\n"
        "    (void)cmds;\n"
        "\n"
        "    i32 width = ctx->window->settings.width;\n"
        "    i32 height = ctx->window->settings.height;\n"
        "    f32 aspect = (f32)width / (f32)height;\n"
        "\n"
        "    mat4 view = {};\n"
        "    mat4 proj = {};\n"
        "    camera_get_view(&game->camera, view);\n"
        "    camera_get_projection(&game->camera, aspect, proj, ctx->renderer_backend);\n"
        "\n"
        "    rl_frame_camera fc = {.valid = true};\n"
        "    glm_mat4_copy(view, fc.view);\n"
        "    glm_mat4_copy(proj, fc.projection);\n"
        "    glm_vec3_copy(game->camera.pos, fc.position);\n"
        "\n"
        "    rl_frame_data frame = {0};\n"
        "    scene_build_frame_data(ctx->scene, &fc, &frame);\n"
        "    renderer_submit_frame_data(&frame);\n"
        "\n"
        "    rl_arena_clear(&game->frame_arena);\n"
        "}\n"
        "\n"
        "void game_destroy(rl_game *game) {\n"
        "    if (!game) return;\n"
        "    rl_arena_deinit(&game->frame_arena);\n"
        "}\n";

    return platform_file_write_all(path, content, cstr_len(content));
}

static b8 write_api_c(const char *root) {
    char path[512];
    cstr_format_buf(path, sizeof(path), "%ssrc/realm_app_api.c", root);

    static const char content[] =
        "#include <realm_app_api.h>\n"
        "#include \"game.h\"\n"
        "\n"
        "#include \"core/logger.h\"\n"
        "\n"
        "u32 realm_app_get_api_version(void) {\n"
        "    return REALM_APP_API_VERSION;\n"
        "}\n"
        "\n"
        "u64 realm_app_get_state_size(void) {\n"
        "    return sizeof(rl_game);\n"
        "}\n"
        "\n"
        "u32 realm_app_get_state_version(void) {\n"
        "    return RL_GAME_STATE_VERSION;\n"
        "}\n"
        "\n"
        "void realm_app_init(void *state, const realm_app_context *ctx) {\n"
        "    rl_game *game = (rl_game *)state;\n"
        "    if (!game_init(game, ctx)) {\n"
        "        RL_ERROR(\"failed to initialize game instance\");\n"
        "    }\n"
        "}\n"
        "\n"
        "void realm_app_update(void *state, const realm_app_context *ctx, realm_app_cmd_queue *cmds, f64 dt) {\n"
        "    rl_game *game = (rl_game *)state;\n"
        "    game_update(game, ctx, cmds, dt);\n"
        "}\n"
        "\n"
        "void realm_app_render(void *state, const realm_app_context *ctx, realm_app_cmd_queue *cmds) {\n"
        "    rl_game *game = (rl_game *)state;\n"
        "    game_render(game, ctx, cmds);\n"
        "}\n"
        "\n"
        "void realm_app_shutdown(void *state, const realm_app_context *ctx) {\n"
        "    (void)ctx;\n"
        "    rl_game *game = (rl_game *)state;\n"
        "    game_destroy(game);\n"
        "}\n";

    return platform_file_write_all(path, content, cstr_len(content));
}

static b8 write_resource_rc(const char *root) {
    char path[512];
    cstr_format_buf(path, sizeof(path), "%sresource.rc", root);

    static const char content[] =
        "// Application icon — resource ID 1\n"
        "1 ICON \"icons/icon.ico\"\n";

    return platform_file_write_all(path, content, cstr_len(content));
}

static b8 write_cmake(const char *root, const char *project_name) {
    char path[512];
    cstr_format_buf(path, sizeof(path), "%sCMakeLists.txt", root);

    char content[2048];
    cstr_format_buf(content, sizeof(content),
        "# %s — game module\n"
        "add_library(realm_app SHARED\n"
        "    src/realm_app_api.c\n"
        "    src/game.c\n"
        ")\n"
        "\n"
        "target_include_directories(realm_app\n"
        "    PRIVATE\n"
        "    ${CMAKE_CURRENT_SOURCE_DIR}/src\n"
        ")\n"
        "\n"
        "target_compile_definitions(realm_app PRIVATE REALM_APP_BUILD=1 ENGINE_SHARED)\n"
        "\n"
        "rl_set_compiler_flags(realm_app)\n"
        "\n"
        "target_link_libraries(realm_app PRIVATE Engine)\n",
        project_name);

    return platform_file_write_all(path, content, cstr_len(content));
}

b8 project_template_generate(const char *root, const char *project_name) {
    char src_dir[512];
    cstr_format_buf(src_dir, sizeof(src_dir), "%ssrc", root);
    if (!platform_dir_create(src_dir)) return false;

    char icons_dir[512];
    cstr_format_buf(icons_dir, sizeof(icons_dir), "%sicons", root);
    if (!platform_dir_create(icons_dir)) return false;

    if (!write_default_scene(root)) return false;
    if (!write_game_h(root)) return false;
    if (!write_game_c(root)) return false;
    if (!write_api_c(root)) return false;
    if (!write_resource_rc(root)) return false;
    if (!write_cmake(root, project_name)) return false;

    return true;
}

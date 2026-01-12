#include "asset/shader.h"

#include "platform/io/file_io.h"
#include "util/str.h"

static SHADER_TYPE infer_shader_type(const char *filename) {
    if (cstr_ends_with(filename, ".vert") || cstr_ends_with(filename, ".vs"))
        return SHADER_TYPE_VERTEX;
    if (cstr_ends_with(filename, ".frag") || cstr_ends_with(filename, ".fs"))
        return SHADER_TYPE_FRAGMENT;
    if (cstr_ends_with(filename, ".comp"))
        return SHADER_TYPE_COMPUTE;
    if (cstr_ends_with(filename, ".geom"))
        return SHADER_TYPE_GEOMETRY;
    if (cstr_ends_with(filename, ".tesc"))
        return SHADER_TYPE_TESS_CONTROL;
    if (cstr_ends_with(filename, ".tese"))
        return SHADER_TYPE_TESS_EVAL;
    return SHADER_TYPE_UNKNOWN;
}


b8 load_shader(rl_arena *arena, rl_asset *asset) {
    rl_temp_arena scratch = rl_arena_scratch_get();

    const char *dir = get_assets_dir(asset->type);
    const char *filename = asset->filename;
    rl_string path = rl_string_format(scratch.arena, "%s%s", dir, filename);

    rl_file shader_file = {};
    platform_file_open(path.cstr, P_FILE_READ, &shader_file);
    platform_file_read_all(&shader_file);

    rl_asset_shader *shader = rl_arena_push(arena, sizeof(rl_asset_shader), alignof(rl_asset_shader));
    // Allocate space for shader text + null terminator
    char *text = rl_arena_push(arena, shader_file.buf_len + 1, alignof(char));
    mem_copy(shader_file.buf, text, shader_file.buf_len);
    text[shader_file.buf_len] = '\0';

    shader->source = text;
    shader->type = infer_shader_type(shader_file.name);
    if (shader->type == SHADER_TYPE_UNKNOWN) {
        RL_ERROR("Failed to load shader file, unsupported file extension");
        return false;
    }

    asset->handle = shader;

    platform_file_close(&shader_file);

    arena_scratch_release(scratch);
    return true;
}
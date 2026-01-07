#include "asset/shader.h"

#include "platform/io/file_io.h"
#include "util/str.h"

b8 load_shader(rl_arena *arena, rl_asset *asset) {
    ARENA_SCRATCH_CREATE(scratch, MiB(5), MEM_ARENA);

    const char *dir = get_assets_dir(asset->type);
    const char *filename = asset->filename;
    rl_string path = rl_string_format(&scratch, "%s%s", dir, filename);

    rl_file shader_file = {};
    platform_file_open(path.cstr, P_FILE_READ, &shader_file);
    platform_file_read_all(&shader_file);

    rl_asset_shader *shader = rl_arena_push(arena, sizeof(rl_asset_shader), alignof(rl_asset_shader));
    // Allocate space for shader text + null terminator
    char *text = rl_arena_push(arena, shader_file.buf_len + 1, alignof(char));
    rl_copy(shader_file.buf, text, shader_file.buf_len);
    text[shader_file.buf_len] = '\0';

    shader->source = text;
    asset->handle = shader;

    platform_file_close(&shader_file);

    ARENA_SCRATCH_DESTROY(&scratch);
    return true;
}
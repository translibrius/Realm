#include "texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "core/logger.h"
#include "memory/arena.h"
#include "platform/io/file_io.h"
#include "util/str.h"
#include "../vendor/stb/stb_image.h"

b8 load_texture(rl_arena *asset_arena, rl_asset *asset) {
    rl_temp_arena scratch = rl_arena_scratch_get();

    const char *dir = get_assets_dir(asset->type);
    const char *filename = asset->filename;
    rl_string path = rl_string_format(scratch.arena, "%s%s", dir, filename);

    i32 width, height, channels;
    u8 *data = stbi_load(path.cstr, &width, &height, &channels, 0);

    if (data == NULL) {
        RL_ERROR("Failed to load texture at '%s'", path.cstr);
        arena_scratch_release(scratch);
        return false;
    }

    rl_texture *texture = rl_arena_push(asset_arena, sizeof(rl_texture), alignof(rl_texture));
    texture->width = width;
    texture->height = height;
    texture->channels = channels;
    u64 size = texture->width * texture->height * texture->channels;
    texture->size = size;

    void *container = rl_arena_push(asset_arena, size, 1);
    mem_copy(data, container, size);
    texture->data = container;

    asset->handle = texture;

    stbi_image_free(data);
    arena_scratch_release(scratch);
    return true;
}
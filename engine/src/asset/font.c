#include "font.h"

#include "../core/logger.h"
#include "platform/io/file_io.h"
#include "asset/asset.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "util/str.h"
#include "vendor/stb/stb_truetype.h"

b8 rl_font_init(rl_arena *asset_arena, rl_asset *asset) {
    ARENA_SCRATCH_CREATE(scratch, MiB(10), MEM_SUBSYSTEM_ASSET);

    RL_DEBUG("Initializing font: %s", asset->filename);

    rl_string path = rl_string_format(&scratch, "%s%s", get_assets_dir(ASSET_FONT), asset->filename);

    rl_file file = {};
    if (!platform_file_open(path.cstr, P_FILE_READ, &file)) {
        RL_ERROR("rl_font_init() failed, failed to open file");
        ARENA_SCRATCH_DESTROY(&scratch);
        return false;
    }

    if (!platform_file_read_all(&file)) {
        RL_ERROR("Failed to read font file");
        ARENA_SCRATCH_DESTROY(&scratch);
        return false;
    }

    /*
    stbtt_fontinfo font_info;
    if (!stbtt_InitFont(&font_info, file.buf, 0)) {
        RL_ERROR("rl_font_init() failed, stb_truetype failed to initialize");
        ARENA_SCRATCH_DESTROY(&scratch);
        return false;
    }
    */

    rl_font *font = rl_arena_alloc(asset_arena, sizeof(rl_font), alignof(rl_font));
    font->data = rl_arena_alloc(asset_arena, file.size, 1);
    rl_copy(file.buf, font->data, file.size);
    font->data_size = file.size;

    font->first_char = 32;
    font->char_count = 95;
    font->baked_px = 24.0f;

    font->atlas_w = 512;
    font->atlas_h = 512;

    u8 *bitmap = rl_arena_alloc(asset_arena, font->atlas_w * font->atlas_h, 1);
    stbtt_bakedchar *char_data = rl_arena_alloc(asset_arena, sizeof(stbtt_bakedchar) * font->char_count, alignof(stbtt_bakedchar));

    i32 result = stbtt_BakeFontBitmap(
        font->data,
        0,
        font->baked_px,
        bitmap,
        font->atlas_w,
        font->atlas_h,
        font->first_char,
        font->char_count,
        char_data
        );

    if (result <= 0) {
        RL_ERROR("Failed to bake font bitmap of file: '%s'", asset->filename);
        ARENA_SCRATCH_DESTROY(&scratch);
        return false;
    }

    font->atlas = bitmap;
    font->chars = char_data;

    asset->handle = font;

    platform_file_close(&file);
    ARENA_SCRATCH_DESTROY(&scratch);
    return true;
}
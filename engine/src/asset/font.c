#include "font.h"

#include "../core/logger.h"
#include "platform/io/file_io.h"
#include "asset/asset.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "util/str.h"
#include "vendor/stb/stb_truetype.h"

b8 rl_font_init(const char *font_name, rl_asset_font *out_asset) {
    rl_arena scratch;
    rl_arena_create(MiB(10), &scratch);

    RL_DEBUG("Initializing font: %s", font_name);

    out_asset->name = font_name;
    rl_string path = rl_string_format(&scratch, "%s%s", get_assets_dir(ASSET_FONT), font_name);
    out_asset->path = path;

    rl_file file = {};
    if (!platform_file_open(path.cstr, P_FILE_READ, &file)) {
        RL_ERROR("rl_font_init() failed, failed to open file");
        return false;
    }

    if (!platform_file_read_all(&file)) {
        RL_ERROR("Failed to read font file");
        return false;
    }

    void *font_buf = rl_arena_alloc(&scratch, file.size, 1);
    rl_copy(file.buf, font_buf, file.size);

    stbtt_fontinfo font_info;
    if (!stbtt_InitFont(&font_info, font_buf, 0)) {
        RL_ERROR("rl_font_init() failed, stb_truetype failed to initialize");
        return false;
    }

    platform_file_close(&file);
    rl_arena_destroy(&scratch);
    return true;
}
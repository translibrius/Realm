#include "font.h"

#include "asset/asset.h"
#include "core/logger.h"
#include "platform/io/file_io.h"

#include "core/font/msdf_wrapper.h"

#include "util/str.h"

b8 rl_font_load(rl_arena *asset_arena, rl_asset *asset) {
    rl_temp_arena scratch = rl_arena_scratch_get();

    RL_DEBUG("Initializing font: %s", asset->filename);

    rl_string path = rl_string_format(scratch.arena, "%s%s", get_assets_dir(ASSET_FONT), asset->filename);

    rl_font *font = rl_arena_push(asset_arena, sizeof(rl_font), alignof(rl_font));
    font->name = asset->filename;
    font->path = path.cstr;
    if (!msdf_load_font_ascii(path.cstr, font)) {
        RL_ERROR("failed to load msdf_font");
        arena_scratch_release(scratch);
        return false;
    }

    asset->handle = font;
    arena_scratch_release(scratch);
    return true;
}

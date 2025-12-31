#include "font.h"

#include "../core/logger.h"
#include "platform/io/file_io.h"
#include "asset/asset.h"

#include "core/font/msdf_wrapper.h"

#include "util/str.h"

b8 rl_font_load(rl_arena *asset_arena, rl_asset *asset) {
    ARENA_SCRATCH_CREATE(scratch, MiB(10), MEM_SUBSYSTEM_ASSET);

    RL_DEBUG("Initializing font: %s", asset->filename);

    rl_string path = rl_string_format(&scratch, "%s%s", get_assets_dir(ASSET_FONT), asset->filename);

    rl_font *font = rl_arena_alloc(asset_arena, sizeof(rl_font), alignof(rl_font));
    font->name = asset->filename;
    font->path = path.cstr;
    if (!msdf_load_font_ascii(path.cstr, font)) {
        RL_ERROR("rl_font_load(): failed to load msdf_font");
        ARENA_SCRATCH_DESTROY(&scratch);
        return false;
    }

    asset->handle = font;
    ARENA_SCRATCH_DESTROY(&scratch);
    return true;
}
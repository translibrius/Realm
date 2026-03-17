#include "font.h"

#include "asset/asset.h"
#include "core/logger.h"
#include "platform/io/file_io.h"

#include "core/font/msdf_wrapper.h"

#include "util/str.h"

b8 rl_font_load(rl_arena *asset_arena, rl_asset *asset) {
    rl_temp_arena scratch = rl_arena_scratch_get();

    RL_DEBUG("Initializing font: %s", asset->filename);

    rl_string path = rl_string_format(scratch.arena, "%s%s", get_asset_root(), asset->source_path);

    rl_font *font = rl_arena_push(asset_arena, sizeof(rl_font), alignof(rl_font));
    font->name = asset->filename;
    font->path = asset->source_path;

    b8 load_ok;
    if (cstr_ends_with(asset->filename, "lucide.ttf")) {
        // Only generate MSDF for codepoints we actually use as icons
        static const u32 lucide_codepoints[] = {
            57410, // arrow-down
            57416, // arrow-left
            57417, // arrow-right
            57418, // arrow-up
            57441, // box
            57452, // check
            57453, // chevron-down
            57455, // chevron-right
            57502, // copy
            57530, // eye
            57536, // file
            57559, // folder
            57577, // grid-3x3
            57590, // image
            57618, // maximize (scale)
            57628, // minus
            57633, // move
            57660, // play
            57661, // plus
            57672, // rotate-ccw
            57681, // search
            57684, // settings
            57720, // sun
            57741, // trash
            57778, // x
            57927, // folder-open
        };
        load_ok = msdf_load_font_codepoints(path.cstr, lucide_codepoints,
            sizeof(lucide_codepoints) / sizeof(lucide_codepoints[0]), font);
    } else {
        load_ok = msdf_load_font_ascii(path.cstr, font);
    }
    if (!load_ok) {
        RL_ERROR("failed to load msdf_font");
        arena_scratch_release(scratch);
        return false;
    }

    // Build ASCII glyph lookup table
    for (u32 i = 0; i < font->glyph_count; i++) {
        u32 cp = (u32)font->glyphs[i].codepoint;
        if (cp < 256) font->glyph_map[cp] = &font->glyphs[i];
    }

    asset->data = font;
    arena_scratch_release(scratch);
    return true;
}

const rl_glyph *rl_font_find_glyph(const rl_font *font, u32 codepoint) {
    for (u32 i = 0; i < font->glyph_count; i++) {
        if ((u32)font->glyphs[i].codepoint == codepoint)
            return &font->glyphs[i];
    }
    return nullptr;
}

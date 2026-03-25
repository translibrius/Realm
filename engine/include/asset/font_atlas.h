#pragma once

#include "defines.h"
#include "asset/texture.h"

// Build a single combined atlas from all loaded font assets.
// Remaps glyph UVs in-place so renderers can bind one texture for all fonts.
// Call once after asset_system_load_engine().
b8 rl_font_atlas_build_combined(void);

// Returns the combined atlas texture, or nullptr if not built yet.
const rl_texture *rl_font_atlas_get_combined(void);

// Free the combined atlas pixel data.
void rl_font_atlas_shutdown(void);

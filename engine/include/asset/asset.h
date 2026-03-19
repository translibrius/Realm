#pragma once

#include "defines.h"

// Opaque handle to a loaded asset. 0 = invalid / not found.
typedef u32 asset_id;

typedef enum ASSET_TYPE {
    ASSET_FONT,
    ASSET_SHADER,
    ASSET_TEXTURE,
    ASSET_MESH,
    ASSET_MODEL,
} ASSET_TYPE;

// Well-known built-in asset paths (used with asset_find)
#define RL_ASSET_FONT_JETBRAINS_MONO      "fonts/JetBrainsMono-Regular.ttf"
#define RL_ASSET_FONT_LUCIDE              "fonts/lucide.ttf"

#define RL_ASSET_SHADER_GL_DEFAULT_VERT   "shaders/opengl/default.vert"
#define RL_ASSET_SHADER_GL_DEFAULT_FRAG   "shaders/opengl/default.frag"
#define RL_ASSET_SHADER_GL_TEXT_VERT      "shaders/opengl/text.vert"
#define RL_ASSET_SHADER_GL_TEXT_FRAG      "shaders/opengl/text.frag"
#define RL_ASSET_SHADER_GL_LIGHT_FRAG     "shaders/opengl/light.frag"
#define RL_ASSET_SHADER_GL_GUI_VERT       "shaders/opengl/gui.vert"
#define RL_ASSET_SHADER_GL_GUI_FRAG       "shaders/opengl/gui.frag"

#define RL_ASSET_SHADER_VK_DEFAULT_VERT   "shaders/vulkan/default.vert"
#define RL_ASSET_SHADER_VK_DEFAULT_FRAG   "shaders/vulkan/default.frag"
#define RL_ASSET_SHADER_VK_TEXT_VERT      "shaders/vulkan/text.vert"
#define RL_ASSET_SHADER_VK_TEXT_FRAG      "shaders/vulkan/text.frag"
#define RL_ASSET_SHADER_VK_GUI_VERT       "shaders/vulkan/gui.vert"
#define RL_ASSET_SHADER_VK_GUI_FRAG       "shaders/vulkan/gui.frag"
#define RL_ASSET_SHADER_VK_LIGHT_FRAG     "shaders/vulkan/light.frag"

#define RL_ASSET_SHADER_GL_GRID_VERT      "shaders/opengl/grid.vert"
#define RL_ASSET_SHADER_GL_GRID_FRAG      "shaders/opengl/grid.frag"
#define RL_ASSET_SHADER_VK_GRID_VERT      "shaders/vulkan/grid.vert"
#define RL_ASSET_SHADER_VK_GRID_FRAG      "shaders/vulkan/grid.frag"

typedef struct rl_asset {
    asset_id id;
    ASSET_TYPE type;
    // Canonical root-relative path (e.g. "shaders/default.vert").
    const char *source_path;
    u32 source_version;
    u64 source_hash;

    const char *filename;
    void *data;
} rl_asset;

REALM_API const char *get_asset_root(void);
REALM_API const char *get_assets_dir(ASSET_TYPE asset_type);

// O(1) lookup by asset id. Returns nullptr if id is 0 or out of range.
REALM_API rl_asset *asset_get(asset_id id);

// Lookup by source_path. Returns 0 if not found.
REALM_API asset_id asset_find(const char *source_path);

// Load a new asset at runtime. Returns its id (or existing id if already loaded).
REALM_API asset_id asset_load(ASSET_TYPE type, const char *source_path);

// Clear content assets (loaded via project_load_assets).
REALM_API void asset_system_clear_content(void);

// Content root override — when set, TEXTURE/MESH assets resolve from this path.
REALM_API void asset_set_content_root(const char *path);
REALM_API void asset_clear_content_root(void);
REALM_API const char *asset_get_resolve_root(ASSET_TYPE type);

#pragma once

#include "defines.h"

typedef enum ASSET_TYPE {
    ASSET_FONT,
    ASSET_SHADER,
    ASSET_TEXTURE,
} ASSET_TYPE;

typedef enum ASSET_ID {
    ASSET_ID_FONT_EVIL_EMPIRE,
    ASSET_ID_FONT_JETBRAINS_MONO_REGULAR,

    ASSET_ID_SHADER_DEFAULT_VERT,
    ASSET_ID_SHADER_TEXT_VERT,
    ASSET_ID_SHADER_DEFAULT_FRAG,
    ASSET_ID_SHADER_TEXT_FRAG,
    ASSET_ID_SHADER_LIGHT_FRAG,
    ASSET_ID_SHADER_VULKAN_TRIANGLE_FRAG,
    ASSET_ID_SHADER_VULKAN_TRIANGLE_VERT,
    ASSET_ID_SHADER_VULKAN_TEXT_VERT,
    ASSET_ID_SHADER_VULKAN_TEXT_FRAG,

    ASSET_ID_TEXTURE_WOOD_CONTAINER,
    ASSET_ID_TEXTURE_WOOD_CONTAINER2,
    ASSET_ID_TEXTURE_FACE,

    ASSET_ID_TOTAL,
} ASSET_ID;

typedef struct rl_asset {
    ASSET_ID id;
    ASSET_TYPE type;
    // Canonical root-relative path (e.g. "shaders/default.vert").
    const char *source_path;
    u32 source_version;
    u64 source_hash;

    const char *filename;
    void *handle;
} rl_asset;

REALM_API const char *get_asset_root(void);
REALM_API const char *get_assets_dir(ASSET_TYPE asset_type);
REALM_API rl_asset *get_asset_by_id(ASSET_ID id);

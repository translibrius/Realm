#pragma once

#include "asset/asset.h"
#include "defines.h"

typedef struct GL_Texture {
    u32 id;
} GL_Texture;

b8 opengl_texture_generate(ASSET_ID asset_id, GL_Texture *out_texture);

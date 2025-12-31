#pragma once

#include "defines.h"

typedef struct GL_Texture {
    u32 id;
} GL_Texture;

b8 opengl_texture_generate(const char *filename, GL_Texture *out_texture);
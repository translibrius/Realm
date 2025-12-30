#pragma once

#include "defines.h"
#include "asset/font.h"

#ifdef __cplusplus
extern "C" {
#endif

b32 msdf_load_font_ascii(const char *path, rl_font *out_font);

#ifdef __cplusplus
}
#endif

#pragma once

#include "defines.h"
#include "gl_types.h"

b8 opengl_text_pipeline_init(GL_TextPipeline *pipeline);
b8 opengl_font_init(const char *filename, GL_Fonts *fonts);

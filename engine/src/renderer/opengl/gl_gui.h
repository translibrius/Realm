#pragma once

#include "defines.h"
#include "renderer/opengl/gl_types.h"

b8 opengl_gui_pipeline_init(GL_Context *ctx);
void opengl_render_gui(void *commands, i32 command_count);

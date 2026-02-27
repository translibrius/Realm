#pragma once

#include "defines.h"
#include "gl_shader.h"

typedef struct rl_ui_draw_list rl_ui_draw_list;
typedef struct GL_Context GL_Context;

typedef struct GL_UIPipeline {
    GL_Shader shader;
    u32 vbo;
    i32 loc_screen_size;
} GL_UIPipeline;

b8 opengl_ui_pipeline_init(GL_Context *ctx);
void opengl_draw_ui(rl_ui_draw_list *list);

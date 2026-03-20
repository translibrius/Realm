#pragma once

#include "defines.h"
#include "renderer/render_target.h"
#include "glad.h"

typedef struct GL_RenderTarget {
    u32 fbo;
    u32 color_texture;
    u32 depth_rbo; // 0 if no depth
    u32 width;
    u32 height;
    rl_rt_format format;
} GL_RenderTarget;

b8  gl_render_target_create(GL_RenderTarget *rt, u32 width, u32 height, rl_rt_format format, b8 with_depth);
void gl_render_target_destroy(GL_RenderTarget *rt);
void gl_render_target_begin(GL_RenderTarget *rt, f32 r, f32 g, f32 b, f32 a);
void gl_render_target_end(void);
void gl_render_target_bind_texture(GL_RenderTarget *rt, u32 slot);
b8  gl_render_target_resize(GL_RenderTarget *rt, u32 width, u32 height);

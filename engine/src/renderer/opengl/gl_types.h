#pragma once

#include "defines.h"
#include "gl_shader.h"
#include "gl_texture.h"
#include "memory/arena.h"
#include "memory/containers/dynamic_array.h"
#include "platform/platform.h"

typedef struct {
    u32 texture_id;
} GL_Font;

DA_DEFINE(GL_Fonts, GL_Font);

typedef struct GL_TextPipeline {
    u32 vao;
    u32 vbo;
    GL_Shader shader;
} GL_TextPipeline;

typedef struct GL_TextVertex {
    vec2 pos;
    vec2 uv;
} GL_TextVertex;

typedef struct opengl_context {
    platform_window *window;
    rl_arena arena;

    // Text
    GL_TextPipeline text_pipeline;
    GL_Fonts fonts;

    // Defaults
    GL_Shader default_shader;
    u32 default_vao;
    GL_Texture wood_texture;
} opengl_context;
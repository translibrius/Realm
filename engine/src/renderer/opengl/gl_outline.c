#include "gl_outline.h"

#include "asset/asset.h"
#include "asset/model.h"
#include "core/logger.h"
#include "gl_mesh.h"
#include "gl_render_target.h"
#include "gl_shader.h"
#include "glad.h"
#include <math.h>

// -------------------------------------------------------------------------
// Fullscreen quad (NDC positions + UV)
// -------------------------------------------------------------------------

static void create_fullscreen_quad(GL_Context *ctx) {
    // clang-format off
    f32 quad[] = {
        // pos        uv
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };
    // clang-format on

    glGenVertexArrays(1, &ctx->fullscreen_quad_vao);
    glGenBuffers(1, &ctx->fullscreen_quad_vbo);
    glBindVertexArray(ctx->fullscreen_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->fullscreen_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    // location 0: vec2 pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *)0);
    // location 1: vec2 uv
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *)(2 * sizeof(f32)));
    glBindVertexArray(0);
}

static void draw_fullscreen_quad(GL_Context *ctx) {
    glBindVertexArray(ctx->fullscreen_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

// -------------------------------------------------------------------------
// Find GL_Mesh for an entity in the frame data
// -------------------------------------------------------------------------

static i32 gl_find_model_cached(GL_Context *ctx, asset_id id) {
    for (u32 i = 0; i < ctx->model_cache_count; i++) {
        if (ctx->model_cache[i].model_id == id) return (i32)i;
    }
    return -1;
}

// -------------------------------------------------------------------------
// Public API
// -------------------------------------------------------------------------

b8 gl_outline_init(GL_Context *ctx, u32 width, u32 height) {
    // Shaders
    if (!opengl_shader_setup(
            asset_find(RL_ASSET_SHADER_GL_OUTLINE_MASK_VERT),
            asset_find(RL_ASSET_SHADER_GL_OUTLINE_MASK_FRAG),
            &ctx->outline_mask_shader)) {
        RL_ERROR("Failed to setup outline mask shader");
        return false;
    }
    if (!opengl_shader_setup(
            asset_find(RL_ASSET_SHADER_GL_JFA_INIT_VERT),
            asset_find(RL_ASSET_SHADER_GL_JFA_INIT_FRAG),
            &ctx->jfa_init_shader)) {
        RL_ERROR("Failed to setup JFA init shader");
        return false;
    }
    if (!opengl_shader_setup(
            asset_find(RL_ASSET_SHADER_GL_JFA_STEP_VERT),
            asset_find(RL_ASSET_SHADER_GL_JFA_STEP_FRAG),
            &ctx->jfa_step_shader)) {
        RL_ERROR("Failed to setup JFA step shader");
        return false;
    }
    if (!opengl_shader_setup(
            asset_find(RL_ASSET_SHADER_GL_OUTLINE_COMP_VERT),
            asset_find(RL_ASSET_SHADER_GL_OUTLINE_COMP_FRAG),
            &ctx->outline_composite_shader)) {
        RL_ERROR("Failed to setup outline composite shader");
        return false;
    }

    // Render targets
    if (!gl_render_target_create(&ctx->outline_mask_rt, width, height, RL_RT_FORMAT_RGBA8, true)) {
        RL_ERROR("Failed to create outline mask RT");
        return false;
    }
    if (!gl_render_target_create(&ctx->outline_jfa_a, width, height, RL_RT_FORMAT_RGBA16F, false)) {
        RL_ERROR("Failed to create JFA A RT");
        return false;
    }
    if (!gl_render_target_create(&ctx->outline_jfa_b, width, height, RL_RT_FORMAT_RGBA16F, false)) {
        RL_ERROR("Failed to create JFA B RT");
        return false;
    }

    // Fullscreen quad
    create_fullscreen_quad(ctx);

    ctx->outline_ready = true;
    return true;
}

void gl_outline_destroy(GL_Context *ctx) {
    gl_render_target_destroy(&ctx->outline_mask_rt);
    gl_render_target_destroy(&ctx->outline_jfa_a);
    gl_render_target_destroy(&ctx->outline_jfa_b);
    if (ctx->fullscreen_quad_vao) { glDeleteVertexArrays(1, &ctx->fullscreen_quad_vao); ctx->fullscreen_quad_vao = 0; }
    if (ctx->fullscreen_quad_vbo) { glDeleteBuffers(1, &ctx->fullscreen_quad_vbo);      ctx->fullscreen_quad_vbo = 0; }
    ctx->outline_ready = false;
}

void gl_outline_resize(GL_Context *ctx, u32 width, u32 height) {
    if (!ctx->outline_ready) return;
    gl_render_target_resize(&ctx->outline_mask_rt, width, height);
    gl_render_target_resize(&ctx->outline_jfa_a, width, height);
    gl_render_target_resize(&ctx->outline_jfa_b, width, height);
}

void gl_outline_render(GL_Context *ctx, rl_frame_data *frame_data) {
    if (!ctx->outline_ready) return;
    if (!frame_data || frame_data->outline_count == 0 || !frame_data->outlines) return;

    // Determine RT dimensions from viewport rect or window
    rl_viewport_rect vr = frame_data->viewport_rect;
    u32 rt_w, rt_h;
    if (vr.w > 0 && vr.h > 0) {
        rt_w = (u32)vr.w;
        rt_h = (u32)vr.h;
    } else {
        rt_w = ctx->window->settings.width;
        rt_h = ctx->window->settings.height;
    }

    // Resize if needed
    gl_outline_resize(ctx, rt_w, rt_h);

    // Disable scissor for offscreen passes — the main renderer's scissor rect is in
    // window coordinates, not FBO-local coordinates, and would clip the mask/JFA textures.
    glDisable(GL_SCISSOR_TEST);

    // For each unique outline group (color+width), we run mask+JFA+composite.
    // First pass: single group (most common — 1-2 outlines with same params).
    // TODO: group by color+width for multiple distinct outlines

    rl_frame_outline *first = &frame_data->outlines[0];

    // === 1. Mask pass — render outlined entities as white on black ===
    gl_render_target_begin(&ctx->outline_mask_rt, 0.0f, 0.0f, 0.0f, 0.0f);
    glEnable(GL_DEPTH_TEST);

    opengl_shader_use(&ctx->outline_mask_shader);
    opengl_shader_set_mat4(&ctx->outline_mask_shader, "view", ctx->view);
    opengl_shader_set_mat4(&ctx->outline_mask_shader, "projection", ctx->projection);

    for (u32 oi = 0; oi < frame_data->outline_count; oi++) {
        rl_frame_outline *ol = &frame_data->outlines[oi];

        if (ol->through_walls) {
            glDisable(GL_DEPTH_TEST);
        }

        // Find matching mesh(es) in frame data by source_entity
        for (u32 mi = 0; mi < frame_data->mesh_count; mi++) {
            rl_frame_mesh *fm = &frame_data->meshes[mi];
            if (fm->source_entity != ol->entity) continue;

            GL_Mesh *draw_mesh = &ctx->cube_mesh;
            if (fm->model_asset) {
                i32 ci = gl_find_model_cached(ctx, fm->model_asset);
                if (ci < 0) continue;
                if (fm->mesh_index >= ctx->model_cache[ci].mesh_count) continue;
                draw_mesh = &ctx->model_cache[ci].meshes[fm->mesh_index];
            }

            opengl_shader_set_mat4(&ctx->outline_mask_shader, "model", fm->model);
            glBindVertexArray(draw_mesh->vao);
            gl_mesh_draw(draw_mesh);
        }

        if (ol->through_walls) {
            glEnable(GL_DEPTH_TEST);
        }
    }

    gl_render_target_end();

    // === 2. JFA init — convert mask to seed coordinates ===
    gl_render_target_begin(&ctx->outline_jfa_a, -1.0f, -1.0f, 0.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);

    opengl_shader_use(&ctx->jfa_init_shader);
    opengl_shader_set_i32(&ctx->jfa_init_shader, "mask_tex", 0);
    gl_render_target_bind_texture(&ctx->outline_mask_rt, 0);
    draw_fullscreen_quad(ctx);

    gl_render_target_end();

    // === 3. JFA flood passes — log2(max_dim) iterations, ping-pong ===
    u32 max_dim = rt_w > rt_h ? rt_w : rt_h;
    i32 passes = (i32)ceilf(log2f((f32)max_dim));
    f32 texel_w = 1.0f / (f32)rt_w;
    f32 texel_h = 1.0f / (f32)rt_h;

    GL_RenderTarget *src = &ctx->outline_jfa_a;
    GL_RenderTarget *dst = &ctx->outline_jfa_b;

    opengl_shader_use(&ctx->jfa_step_shader);
    opengl_shader_set_i32(&ctx->jfa_step_shader, "jfa_tex", 0);
    opengl_shader_set_vec2(&ctx->jfa_step_shader, "texel_size", (vec2){texel_w, texel_h});

    for (i32 i = passes - 1; i >= 0; i--) {
        f32 step = (f32)(1 << i);

        gl_render_target_begin(dst, 0.0f, 0.0f, 0.0f, 1.0f);
        glDisable(GL_DEPTH_TEST);

        opengl_shader_set_f32(&ctx->jfa_step_shader, "step_size", step);
        gl_render_target_bind_texture(src, 0);
        draw_fullscreen_quad(ctx);

        gl_render_target_end();

        // Swap
        GL_RenderTarget *tmp = src;
        src = dst;
        dst = tmp;
    }

    // === 4. Composite — blend outline onto scene ===
    // Restore the viewport that was active before outlines
    b8 has_viewport = (vr.w > 0 && vr.h > 0);
    if (has_viewport) {
        i32 win_h = ctx->window->settings.height;
        i32 gl_y = win_h - (i32)(vr.y + vr.h);
        glViewport((i32)vr.x, gl_y, (i32)vr.w, (i32)vr.h);
        glEnable(GL_SCISSOR_TEST);
        glScissor((i32)vr.x, gl_y, (i32)vr.w, (i32)vr.h);
    } else {
        glViewport(0, 0, ctx->window->settings.width, ctx->window->settings.height);
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    opengl_shader_use(&ctx->outline_composite_shader);
    opengl_shader_set_i32(&ctx->outline_composite_shader, "jfa_tex", 0);
    opengl_shader_set_i32(&ctx->outline_composite_shader, "mask_tex", 1);
    opengl_shader_set_vec4(&ctx->outline_composite_shader, "outline_color", first->color);
    opengl_shader_set_f32(&ctx->outline_composite_shader, "outline_width", first->width);
    opengl_shader_set_vec2(&ctx->outline_composite_shader, "screen_size", (vec2){(f32)rt_w, (f32)rt_h});

    gl_render_target_bind_texture(src, 0);
    gl_render_target_bind_texture(&ctx->outline_mask_rt, 1);
    draw_fullscreen_quad(ctx);

    // Restore state
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    if (has_viewport) {
        glDisable(GL_SCISSOR_TEST);
    }
}

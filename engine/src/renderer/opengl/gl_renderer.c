#include "renderer/opengl/gl_renderer.h"

#include "asset/asset.h"
#include "asset/mesh.h"
#include "gl_gui.h"
#include "math/ray.h"
#include "gl_texture.h"
#include "renderer/opengl/gl_text.h"
#include "core/logger.h"
#include "platform/platform.h"
#include "glad.h"
#include "core/event.h"
#include "renderer/opengl/gl_shader.h"
#include "renderer/opengl/gl_types.h"
#include "core/camera.h"

static GL_Context context;

static GL_Texture *gl_find_texture(asset_id id) {
    for (u32 i = 0; i < context.texture_count; i++) {
        if (context.textures[i].asset_id == id) {
            return &context.textures[i].texture;
        }
    }
    return nullptr;
}

static b8 gl_load_texture(asset_id id) {
    if (context.texture_count >= 64) {
        RL_ERROR("GL texture table full");
        return false;
    }
    GL_Texture tex = {0};
    if (!opengl_texture_generate(id, &tex)) {
        return false;
    }
    context.textures[context.texture_count].asset_id = id;
    context.textures[context.texture_count].texture = tex;
    context.texture_count++;
    return true;
}

static GL_Mesh *gl_find_mesh(asset_id id) {
    for (u32 i = 0; i < context.mesh_cache_count; i++) {
        if (context.mesh_cache[i].asset_id == id) {
            return &context.mesh_cache[i].mesh;
        }
    }
    return nullptr;
}

static GL_Mesh *gl_ensure_mesh(asset_id id) {
    GL_Mesh *existing = gl_find_mesh(id);
    if (existing) {
        return existing;
    }

    rl_asset *asset = asset_get(id);
    if (!asset || asset->type != ASSET_MESH || !asset->data) {
        RL_ERROR("gl_ensure_mesh: invalid mesh asset %u", id);
        return nullptr;
    }

    if (context.mesh_cache_count >= 64) {
        RL_ERROR("GL mesh cache full");
        return nullptr;
    }

    rl_mesh *mesh = (rl_mesh *)asset->data;
    if (mesh->primitive_count == 0) {
        RL_ERROR("gl_ensure_mesh: mesh has no primitives");
        return nullptr;
    }

    rl_mesh_primitive *prim = &mesh->primitives[0];
    GL_Mesh gl_mesh = gl_mesh_create_from_primitive(
        prim->vertices, prim->vertex_count,
        prim->indices, prim->index_count
    );

    // Ensure the mesh's diffuse texture is uploaded to GL
    if (mesh->material_count > 0 && prim->material_index < mesh->material_count) {
        asset_id tex_id = mesh->materials[prim->material_index].base_color_texture;
        if (tex_id && !gl_find_texture(tex_id)) {
            gl_load_texture(tex_id);
        }
    }

    u32 idx = context.mesh_cache_count++;
    context.mesh_cache[idx].asset_id = id;
    context.mesh_cache[idx].mesh = gl_mesh;

    RL_DEBUG("Uploaded mesh asset %u to GPU (verts=%u, indices=%u)", id, prim->vertex_count, prim->index_count);
    return &context.mesh_cache[idx].mesh;
}

GL_Context *opengl_get_context(void) {
    return &context;
}

void opengl_resize_framebuffer(i32 w, i32 h) {
    glViewport(0, 0, w, h);
}

void opengl_set_view_projection(mat4 view, mat4 projection, vec3 pos) {
    glm_mat4_copy(view, context.view);
    glm_mat4_copy(projection, context.projection);
    glm_vec3_copy(pos, context.pos);
}

void opengl_submit_frame_data(rl_frame_data *frame_data) {
    if (!frame_data) {
        return;
    }

    if (frame_data->camera.valid) {
        glm_mat4_copy(frame_data->camera.view, context.view);
        glm_mat4_copy(frame_data->camera.projection, context.projection);
        glm_vec3_copy(frame_data->camera.position, context.pos);
    }

    // Constrain 3D rendering to viewport rect if specified
    rl_viewport_rect vr = frame_data->viewport_rect;
    b8 has_viewport = (vr.w > 0 && vr.h > 0);
    if (has_viewport) {
        i32 win_h = context.window->settings.height;
        i32 gl_y = win_h - (i32)(vr.y + vr.h);
        glViewport((i32)vr.x, gl_y, (i32)vr.w, (i32)vr.h);
        glEnable(GL_SCISSOR_TEST);
        glScissor((i32)vr.x, gl_y, (i32)vr.w, (i32)vr.h);
        glStencilMask(0xFF);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glStencilMask(0x00);
    }

    // --- Grid pass ---
    if (frame_data->show_grid && frame_data->camera.valid) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);

        opengl_shader_use(&context.grid_shader);
        opengl_shader_set_mat4(&context.grid_shader, "view", context.view);
        opengl_shader_set_mat4(&context.grid_shader, "projection", context.projection);
        opengl_shader_set_vec3(&context.grid_shader, "camera_pos", context.pos);

        glBindVertexArray(context.grid_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
    }

    if (!frame_data->meshes || frame_data->mesh_count == 0) {
        goto text_pass;
    }

    rl_frame_point_light light = RL_DEFAULT_POINT_LIGHT;
    if (frame_data->point_light_count > 0 && frame_data->point_lights) {
        light = frame_data->point_lights[0];
    }

    b8 wireframe_on = context.debug_wireframe;
    if (wireframe_on) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_CULL_FACE);
    }

    // --- Lit pass: bind shader + shared uniforms once ---
    opengl_shader_use(&context.default_shader);
    opengl_shader_set_mat4(&context.default_shader, "view", context.view);
    opengl_shader_set_mat4(&context.default_shader, "projection", context.projection);
    opengl_shader_set_vec3(&context.default_shader, "view_pos", context.pos);
    opengl_shader_set_i32(&context.default_shader, "material.diffuse", 0);
    opengl_shader_set_vec3(&context.default_shader, "light.position", light.position);
    opengl_shader_set_vec3(&context.default_shader, "light.ambient", light.ambient);
    opengl_shader_set_vec3(&context.default_shader, "light.diffuse", light.diffuse);
    opengl_shader_set_vec3(&context.default_shader, "light.specular", light.specular);
    glActiveTexture(GL_TEXTURE0);

    u32 bound_tex = 0;
    u32 bound_vao = 0;

    for (u32 i = 0; i < frame_data->mesh_count; i++) {
        rl_frame_mesh *fm = &frame_data->meshes[i];
        if (fm->kind != RL_FRAME_MESH_KIND_LIT) {
            continue;
        }

        GL_Mesh *draw_mesh = &context.cube_mesh;
        asset_id diffuse_id = fm->material.diffuse_map;

        if (fm->mesh_asset) {
            draw_mesh = gl_ensure_mesh(fm->mesh_asset);
            if (!draw_mesh) continue;

            // Fall back to the mesh's own texture if game didn't set one
            if (!diffuse_id) {
                rl_asset *mesh_asset = asset_get(fm->mesh_asset);
                rl_mesh *m = (rl_mesh *)mesh_asset->data;
                if (m->material_count > 0) {
                    diffuse_id = m->materials[0].base_color_texture;
                }
            }
        }

        if (draw_mesh->vao != bound_vao) {
            glBindVertexArray(draw_mesh->vao);
            bound_vao = draw_mesh->vao;
        }

        GL_Texture *tex = diffuse_id ? gl_find_texture(diffuse_id) : nullptr;
        if (!tex && diffuse_id) {
            gl_load_texture(diffuse_id);
            tex = gl_find_texture(diffuse_id);
        }
        u32 tex_id = tex ? tex->id : 0;
        if (tex_id && tex_id != bound_tex) {
            glBindTexture(GL_TEXTURE_2D, tex_id);
            bound_tex = tex_id;
        }

        opengl_shader_set_vec3(&context.default_shader, "material.specular", fm->material.specular);
        opengl_shader_set_f32(&context.default_shader, "material.shininess", fm->material.shininess);
        opengl_shader_set_mat4(&context.default_shader, "model", fm->model);

        if (!context.debug_wireframe && fm->wireframe != wireframe_on) {
            glPolygonMode(GL_FRONT_AND_BACK, fm->wireframe ? GL_LINE : GL_FILL);
            if (fm->wireframe) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
            wireframe_on = fm->wireframe;
        }

        gl_mesh_draw(draw_mesh);
    }

    // --- Unlit pass: bind shader + shared uniforms once ---
    opengl_shader_use(&context.light_shader);
    opengl_shader_set_mat4(&context.light_shader, "view", context.view);
    opengl_shader_set_mat4(&context.light_shader, "projection", context.projection);

    bound_vao = 0;

    for (u32 i = 0; i < frame_data->mesh_count; i++) {
        rl_frame_mesh *fm = &frame_data->meshes[i];
        if (fm->kind != RL_FRAME_MESH_KIND_UNLIT) {
            continue;
        }

        GL_Mesh *draw_mesh = &context.cube_mesh;
        if (fm->mesh_asset) {
            draw_mesh = gl_ensure_mesh(fm->mesh_asset);
            if (!draw_mesh) continue;
        }

        if (draw_mesh->vao != bound_vao) {
            glBindVertexArray(draw_mesh->vao);
            bound_vao = draw_mesh->vao;
        }

        opengl_shader_set_vec3(&context.light_shader, "flat_color", fm->material.specular);
        opengl_shader_set_mat4(&context.light_shader, "model", fm->model);

        if (!context.debug_wireframe && fm->wireframe != wireframe_on) {
            glPolygonMode(GL_FRONT_AND_BACK, fm->wireframe ? GL_LINE : GL_FILL);
            if (fm->wireframe) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
            wireframe_on = fm->wireframe;
        }

        gl_mesh_draw(draw_mesh);
    }

    if (wireframe_on) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_CULL_FACE);
    }

    // --- World-space overlays (transform gizmos — no depth test, main camera) ---
    if (frame_data->world_overlay_count > 0 && frame_data->world_overlays) {
        glDisable(GL_DEPTH_TEST);

        // light_shader + scene view/projection already set from unlit pass
        bound_vao = 0;
        for (u32 i = 0; i < frame_data->world_overlay_count; i++) {
            rl_frame_mesh *wm = &frame_data->world_overlays[i];
            GL_Mesh *draw_mesh = &context.cube_mesh;
            if (draw_mesh->vao != bound_vao) {
                glBindVertexArray(draw_mesh->vao);
                bound_vao = draw_mesh->vao;
            }
            opengl_shader_set_vec3(&context.light_shader, "flat_color", wm->material.specular);
            opengl_shader_set_mat4(&context.light_shader, "model", wm->model);
            gl_mesh_draw(draw_mesh);
        }

        glEnable(GL_DEPTH_TEST);
    }

    // --- Overlay pass (gizmo axes, etc.) ---
    if (frame_data->overlay_count > 0 && frame_data->overlay_meshes && frame_data->overlay_camera.valid) {
        // Compute gizmo sub-viewport: 100x100 in bottom-left of viewport rect, 10px margin
        f32 gx, gy, gw, gh;
        gw = 100.0f;
        gh = 100.0f;
        if (has_viewport) {
            gx = vr.x + 10.0f;
            gy = vr.y + vr.h - gh - 10.0f;
        } else {
            gx = 10.0f;
            gy = (f32)context.window->settings.height - gh - 10.0f;
        }
        i32 win_h = context.window->settings.height;
        i32 gl_gy = win_h - (i32)(gy + gh);
        glViewport((i32)gx, gl_gy, (i32)gw, (i32)gh);
        glDisable(GL_DEPTH_TEST);

        opengl_shader_use(&context.light_shader);
        opengl_shader_set_mat4(&context.light_shader, "view", frame_data->overlay_camera.view);
        opengl_shader_set_mat4(&context.light_shader, "projection", frame_data->overlay_camera.projection);

        glBindVertexArray(context.cube_mesh.vao);
        for (u32 i = 0; i < frame_data->overlay_count; i++) {
            rl_frame_mesh *om = &frame_data->overlay_meshes[i];
            opengl_shader_set_vec3(&context.light_shader, "flat_color", om->material.specular);
            opengl_shader_set_mat4(&context.light_shader, "model", om->model);
            gl_mesh_draw(&context.cube_mesh);
        }

        glEnable(GL_DEPTH_TEST);

        // Restore scene viewport before text pass
        if (has_viewport) {
            i32 gl_y = win_h - (i32)(vr.y + vr.h);
            glViewport((i32)vr.x, gl_y, (i32)vr.w, (i32)vr.h);
        }
    }

text_pass:
    // Restore full-window viewport after 3D passes
    if (has_viewport) {
        glViewport(0, 0, context.window->settings.width, context.window->settings.height);
        glDisable(GL_SCISSOR_TEST);
    }

    if (frame_data->texts && frame_data->text_count > 0) {
        opengl_render_text_batch(frame_data->texts, frame_data->text_count);
    }
}

b8 opengl_initialize(platform_window *platform_window, b8 vsync) {
    context.window = platform_window;
    context.clear_color[0] = RL_CLEAR_COLOR_R;
    context.clear_color[1] = RL_CLEAR_COLOR_G;
    context.clear_color[2] = RL_CLEAR_COLOR_B;
    context.clear_color[3] = RL_CLEAR_COLOR_A;

    da_init(&context.fonts);
    rl_arena_init(&context.arena, MiB(100), MiB(25), MEM_SUBSYSTEM_RENDERER);

    RL_INFO("Initializing Renderer: OpenGL");
    if (!platform_create_opengl_context(context.window)) {
        RL_ERROR("opengl_initialize() failed: Failed to create OpenGL context");
        return false;
    }

    platform_context_make_current(context.window);
    platform_set_vsync(context.window, vsync);
    opengl_resize_framebuffer(context.window->settings.width, context.window->settings.height);

    // Shader init
    if (!opengl_shader_setup(asset_find(RL_ASSET_SHADER_GL_DEFAULT_VERT), asset_find(RL_ASSET_SHADER_GL_DEFAULT_FRAG), &context.default_shader)) {
        RL_ERROR("opengl_shader_setup() failed");
        return false;
    }
    if (!opengl_shader_setup(asset_find(RL_ASSET_SHADER_GL_DEFAULT_VERT), asset_find(RL_ASSET_SHADER_GL_LIGHT_FRAG), &context.light_shader)) {
        RL_ERROR("opengl_shader_setup() failed");
        return false;
    }

    // Text pipeline
    opengl_text_pipeline_init(&context);

    // GUI pipeline
    if (!opengl_gui_pipeline_init(&context)) {
        RL_ERROR("opengl_gui_pipeline_init() failed");
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    context.cube_mesh = gl_mesh_create_cube();

    // Grid pipeline
    if (!opengl_shader_setup(asset_find(RL_ASSET_SHADER_GL_GRID_VERT), asset_find(RL_ASSET_SHADER_GL_GRID_FRAG), &context.grid_shader)) {
        RL_ERROR("Grid shader setup failed");
        return false;
    }
    glGenVertexArrays(1, &context.grid_vao);

    return true;
}

void opengl_destroy() {
    for (u32 i = 0; i < context.mesh_cache_count; i++) {
        gl_mesh_destroy(&context.mesh_cache[i].mesh);
    }
    context.mesh_cache_count = 0;

    for (u32 i = 0; i < context.texture_count; i++) {
        glDeleteTextures(1, &context.textures[i].texture.id);
    }
    context.texture_count = 0;

    gl_mesh_destroy(&context.cube_mesh);
    if (context.grid_vao) glDeleteVertexArrays(1, &context.grid_vao);
    rl_arena_deinit(&context.arena);
}

void opengl_begin_frame(f64 delta_time) {
    (void)delta_time;

    glClearColor(context.clear_color[0], context.clear_color[1], context.clear_color[2], context.clear_color[3]);
    glStencilMask(0xFF);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glStencilMask(0x00);
}

void opengl_end_frame() {
}

void opengl_swap_buffers() {
    platform_swap_buffers(context.window);
}

void opengl_set_vsync(b8 vsync) {
    platform_set_vsync(context.window, vsync);
}

platform_window *opengl_get_active_window() {
    return context.window;
}

void opengl_set_active_window(platform_window *window) {
    context.window = window;
}

void opengl_set_wireframe(b8 enabled) {
    context.debug_wireframe = enabled;
}

void opengl_set_clear_color(f32 r, f32 g, f32 b, f32 a) {
    context.clear_color[0] = r;
    context.clear_color[1] = g;
    context.clear_color[2] = b;
    context.clear_color[3] = a;
}

#include "ed_asset_browser.h"

#include "ed_application.h"
#include "asset/asset.h"
#include "core/component.h"
#include "core/entity.h"
#include "core/logger.h"
#include "core/project.h"
#include "core/scene.h"
#include "gui/gui_clay.h"
#include "gui/gui_panel.h"
#include "gui/gui_scroll.h"
#include "gui/gui_text.h"
#include "gui/gui_theme.h"
#include "gui/gui_tree.h"
#include "memory/arena.h"
#include "platform/io/file_scan.h"
#include "util/str.h"

#include <stdio.h>

#define ED_ASSET_NODE_BASE 0x20000u
#define ED_ASSET_TEX_BASE  (ED_ASSET_NODE_BASE + 1)
#define ED_ASSET_MDL_BASE  (ED_ASSET_NODE_BASE + 0x1000u)

void ed_asset_browser_init(ed_asset_browser *browser) {
    if (!browser) return;
    *browser = (ed_asset_browser){0};
    browser->scroll = (gui_scroll_state){.auto_scroll = false};
    da_init(&browser->textures);
    da_init(&browser->models);
}

void ed_asset_browser_shutdown(ed_asset_browser *browser) {
    if (!browser) return;
    da_free(&browser->textures);
    da_free(&browser->models);
}

void ed_asset_browser_refresh(ed_asset_browser *browser) {
    if (!browser) return;

    rl_project *proj = project_get();
    if (!proj) return;

    // Clear previous
    da_free(&browser->textures);
    da_free(&browser->models);
    da_init(&browser->textures);
    da_init(&browser->models);

    ARENA_SCRATCH_START();

    char tex_path[512];
    snprintf(tex_path, sizeof(tex_path), "%stextures/", proj->asset_path);
    platform_dir_scan(tex_path, ".jpg,.jpeg,.png,.bmp,.tga", scratch.arena, &browser->textures);

    char mdl_path[512];
    snprintf(mdl_path, sizeof(mdl_path), "%smodels/", proj->asset_path);
    platform_dir_scan(mdl_path, ".gltf,.glb,.obj", scratch.arena, &browser->models);

    // Note: filenames point into scratch arena which will be released.
    // We need to copy them. Re-scan using a persistent approach:
    // Actually, the DA items' name pointers point into the scratch arena.
    // We must copy them before releasing. Let's do it now.

    // Copy texture names
    for (u64 i = 0; i < browser->textures.count; i++) {
        u32 len = cstr_len(browser->textures.items[i].name);
        char *copy = mem_alloc(len + 1, MEM_STRING);
        mem_copy(copy, browser->textures.items[i].name, len + 1);
        browser->textures.items[i].name = copy;
    }

    // Copy model names
    for (u64 i = 0; i < browser->models.count; i++) {
        u32 len = cstr_len(browser->models.items[i].name);
        char *copy = mem_alloc(len + 1, MEM_STRING);
        mem_copy(copy, browser->models.items[i].name, len + 1);
        browser->models.items[i].name = copy;
    }

    ARENA_SCRATCH_RELEASE();

    browser->needs_refresh = false;
}

static void free_entry_names(DirEntries *entries) {
    for (u64 i = 0; i < entries->count; i++) {
        if (entries->items[i].name) {
            u32 len = cstr_len(entries->items[i].name);
            mem_free((void *)entries->items[i].name, len + 1, MEM_STRING);
        }
    }
}

void ed_asset_browser_render(ed_asset_browser *browser, ed_application *app, f32 width, f32 height) {
    if (!browser || !app) return;

    if (browser->needs_refresh) {
        // Free old names before refresh
        free_entry_names(&browser->textures);
        free_entry_names(&browser->models);
        ed_asset_browser_refresh(browser);
    }

    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg header_text = {.color = t->text, .size = 13, .font = font};

    gui_panel_cfg panel = {
        .color = t->bg,
        .width_sizing = (width > 0) ? GUI_SIZE_FIXED : GUI_SIZE_GROW,
        .width = width,
        .height_sizing = (height > 0) ? GUI_SIZE_FIXED : GUI_SIZE_GROW,
        .height = height,
        .padding = 8,
        .gap = 4,
    };
    GUI_PANEL(&panel) {
        gui_text("Assets", &header_text);
        gui_separator();

        gui_scroll_begin(&browser->scroll, &(gui_scroll_cfg){
            .scrollbar_width = 6, .thumb_radius = 3,
        });

        gui_tree_cfg tcfg = {.font = font};
        gui_tree_begin(&browser->tree, &tcfg);
        {
            // Textures node
            static b8 tex_expanded = true;
            gui_tree_node_result tex_r = gui_tree_node_begin(
                ED_ASSET_NODE_BASE, "Textures", &tex_expanded, false);
            if (tex_r.expanded) {
                for (u64 i = 0; i < browser->textures.count; i++) {
                    if (browser->textures.items[i].is_dir) continue;
                    u32 node_id = ED_ASSET_TEX_BASE + (u32)i;
                    gui_tree_node_begin(node_id, browser->textures.items[i].name, nullptr, true);
                    gui_tree_node_end();
                }
            }
            gui_tree_node_end();

            // Models node
            static b8 mdl_expanded = true;
            gui_tree_node_result mdl_r = gui_tree_node_begin(
                ED_ASSET_MDL_BASE - 1, "Models", &mdl_expanded, false);
            if (mdl_r.expanded) {
                for (u64 i = 0; i < browser->models.count; i++) {
                    u32 node_id = ED_ASSET_MDL_BASE + (u32)i;
                    const char *name = browser->models.items[i].name;
                    b8 is_dir = browser->models.items[i].is_dir;

                    gui_tree_node_result nr = gui_tree_node_begin(node_id, name, nullptr, true);
                    gui_tree_node_end();

                    // Double-click on a mesh entry: create entity in scene
                    if (nr.selected && !is_dir && app->scene) {
                        char rel_path[512];
                        snprintf(rel_path, sizeof(rel_path), "models/%s", name);
                        asset_id aid = asset_find(rel_path);
                        if (aid) {
                            rl_entity e = scene_entity_create(app->scene, name);
                            transform_add(&app->scene->components, e);
                            rl_mesh_component *mc = mesh_add(&app->scene->components, e);
                            if (mc) {
                                mc->mesh_asset = aid;
                                mc->kind = RL_FRAME_MESH_KIND_LIT;
                            }
                            app->scene_dirty = true;
                        }
                    }
                }
            }
            gui_tree_node_end();
        }
        gui_tree_end();

        gui_scroll_end();
    }
}

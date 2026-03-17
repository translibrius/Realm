#include "ed_inspector.h"

#include "ed_undo.h"
#include "asset/asset.h"
#include "core/component.h"
#include "gui/gui_clay.h"
#include "gui/gui_field.h"
#include "gui/gui_focus.h"
#include "gui/gui_panel.h"
#include "gui/gui_number_input.h"
#include "gui/gui_panel.h"
#include "gui/gui_text.h"
#include "gui/gui_text_input.h"
#include "gui/gui_theme.h"
#include <string.h>

// ── Init / bind ─────────────────────────────────────────────────────────────

void ed_inspector_init(ed_inspector *insp, ed_undo_stack *undo) {
    if (!insp) return;
    memset(insp, 0, sizeof(*insp));
    insp->bound_entity_idx = 0;
    insp->mesh_kind = (gui_dropdown_state){.selected = 0};
    insp->undo = undo;
}

static void cancel_editing(ed_inspector *insp) {
    gui_focus_clear();

    // Cancel any active editing on number inputs
    for (u32 t = 0; t < 3; t++) {
        insp->transform[t].x.editing = false;
        insp->transform[t].y.editing = false;
        insp->transform[t].z.editing = false;
    }
    for (u32 l = 0; l < 3; l++) {
        insp->light[l].x.editing = false;
        insp->light[l].y.editing = false;
        insp->light[l].z.editing = false;
    }
    insp->mat_specular.x.editing = false;
    insp->mat_specular.y.editing = false;
    insp->mat_specular.z.editing = false;
    insp->mat_shininess.editing = false;
}

void ed_inspector_bind(ed_inspector *insp, rl_scene *scene, rl_entity entity) {
    if (!insp || !scene) return;

    cancel_editing(insp);

    u32 idx = rl_entity_index(entity);
    insp->bound_entity_idx = idx;
    rl_component_store *cs = &scene->components;

    // Name
    rl_name_component *nc = name_get(cs, entity);
    if (nc) {
        u16 len = (u16)strlen(nc->name);
        if (len >= GUI_TEXT_INPUT_MAX) len = GUI_TEXT_INPUT_MAX - 1;
        memcpy(insp->name_input.buf, nc->name, len);
        insp->name_input.buf[len] = '\0';
        insp->name_input.len = len;
        insp->name_input.cursor = len;
    } else {
        insp->name_input.buf[0] = '\0';
        insp->name_input.len = 0;
        insp->name_input.cursor = 0;
    }

    // Transform
    rl_transform *tr = transform_get(cs, entity);
    if (tr) {
        insp->transform[0].x.value = tr->position[0];
        insp->transform[0].y.value = tr->position[1];
        insp->transform[0].z.value = tr->position[2];
        insp->transform[1].x.value = tr->rotation[0];
        insp->transform[1].y.value = tr->rotation[1];
        insp->transform[1].z.value = tr->rotation[2];
        insp->transform[2].x.value = tr->scale[0];
        insp->transform[2].y.value = tr->scale[1];
        insp->transform[2].z.value = tr->scale[2];
    }

    // Mesh
    rl_mesh_component *mc = mesh_get(cs, entity);
    if (mc) {
        insp->mesh_kind.selected = (i32)mc->kind;
        insp->wireframe = mc->wireframe;
        insp->mat_specular.x.value = mc->material.specular[0];
        insp->mat_specular.y.value = mc->material.specular[1];
        insp->mat_specular.z.value = mc->material.specular[2];
        insp->mat_shininess.value  = mc->material.shininess;
    }

    // Light
    rl_light_component *lc = light_get(cs, entity);
    if (lc) {
        insp->light[0].x.value = lc->ambient[0];
        insp->light[0].y.value = lc->ambient[1];
        insp->light[0].z.value = lc->ambient[2];
        insp->light[1].x.value = lc->diffuse[0];
        insp->light[1].y.value = lc->diffuse[1];
        insp->light[1].z.value = lc->diffuse[2];
        insp->light[2].x.value = lc->specular[0];
        insp->light[2].y.value = lc->specular[1];
        insp->light[2].z.value = lc->specular[2];
    }
}

// ── Helpers ─────────────────────────────────────────────────────────────────

static b8 vec3_row(ed_inspector_vec3 *v, const gui_number_input_cfg *cfg, f32 dt) {
    b8 changed = false;
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));

    GUI_ROW(2) {
        gui_textn("X", 1, &(gui_text_cfg){.color = GUI_RGB(220, 80, 80), .size = 12, .font = font});
        changed |= gui_number_input(&v->x, cfg, dt);
        gui_textn("Y", 1, &(gui_text_cfg){.color = GUI_RGB(80, 200, 80), .size = 12, .font = font});
        changed |= gui_number_input(&v->y, cfg, dt);
        gui_textn("Z", 1, &(gui_text_cfg){.color = GUI_RGB(80, 130, 220), .size = 12, .font = font});
        changed |= gui_number_input(&v->z, cfg, dt);
    }
    return changed;
}

// ── Section header ──────────────────────────────────────────────────────────

static void section_header(const char *label, const gui_theme *t, u16 font) {
    gui_spacer_fixed(4);
    gui_panel_cfg hdr = {
        .color = t->bg_secondary,
        .width_sizing = GUI_SIZE_GROW,
        .padding = 5,
        .corner_radius = 3,
    };
    GUI_PANEL(&hdr) {
        gui_text(label, &(gui_text_cfg){.color = t->text, .size = 12, .font = font});
    }
    gui_spacer_fixed(2);
}

// ── Render ──────────────────────────────────────────────────────────────────

b8 ed_inspector_render(ed_inspector *insp, rl_scene *scene, rl_entity entity,
                       b8 *scene_dirty, f32 dt) {
    if (!insp || !scene) return false;

    rl_component_store *cs = &scene->components;
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    const gui_theme *t = gui_theme_get();
    gui_text_cfg header_text = {.color = t->text, .size = 13, .font = font};
    gui_text_cfg dim_text = {.color = t->text_dim, .size = 12, .font = font};

    gui_field_cfg fcfg = {.label_width = 70, .font = font, .font_size = 12, .label_color = t->text_dim};

    b8 any_changed = false;

    // ── Name ────────────────────────────────────────────────────────────
    rl_name_component *nc = name_get(cs, entity);
    if (nc) {
        b8 name_editing = gui_focus_is(insp->name_input._id);
        if (!name_editing) {
            gui_text(nc->name, &header_text);
        } else {
            gui_text_input_render(&insp->name_input, dt, &(gui_text_input_render_cfg){
                .bg_color = t->bg_input,
                .text_color = t->text,
                .border_color = t->border,
                .border_width = 1,
                .padding = 4,
                .height = 24,
                .font = font,
                .font_size = 13,
            });
        }

        // When focus leaves the name input, write back if changed
        if (!name_editing && insp->name_input.len > 0) {
            if (strcmp(nc->name, insp->name_input.buf) != 0) {
                rl_name_component before = *nc;
                u16 copy_len = insp->name_input.len;
                if (copy_len >= RL_NAME_MAX) copy_len = RL_NAME_MAX - 1;
                memcpy(nc->name, insp->name_input.buf, copy_len);
                nc->name[copy_len] = '\0';
                if (insp->undo) {
                    ed_undo_entry entry = {.action = ED_UNDO_NAME, .entity = entity};
                    entry.name.before = before;
                    entry.name.after = *nc;
                    ed_undo_push(insp->undo, &entry);
                }
                any_changed = true;
            }
        }
        gui_separator();
    }

    // ── Transform ───────────────────────────────────────────────────────
    rl_transform *tr = transform_get(cs, entity);
    if (tr) {
        section_header("Transform", t, font);

        gui_number_input_cfg pos_cfg = {.step = 0.01f, .format = "%.2f", .width = 50, .height = 18};
        gui_number_input_cfg rot_cfg = {.step = 0.5f, .format = "%.1f", .width = 50, .height = 18};
        gui_number_input_cfg scl_cfg = {.step = 0.01f, .format = "%.2f", .width = 50, .height = 18};

        gui_field_begin("Position", &fcfg);
        b8 pos_changed = vec3_row(&insp->transform[0], &pos_cfg, dt);
        gui_field_end();

        gui_field_begin("Rotation", &fcfg);
        b8 rot_changed = vec3_row(&insp->transform[1], &rot_cfg, dt);
        gui_field_end();

        gui_field_begin("Scale", &fcfg);
        b8 scl_changed = vec3_row(&insp->transform[2], &scl_cfg, dt);
        gui_field_end();

        if (pos_changed || rot_changed || scl_changed) {
            tr->position[0] = insp->transform[0].x.value;
            tr->position[1] = insp->transform[0].y.value;
            tr->position[2] = insp->transform[0].z.value;
            tr->rotation[0] = insp->transform[1].x.value;
            tr->rotation[1] = insp->transform[1].y.value;
            tr->rotation[2] = insp->transform[1].z.value;
            tr->scale[0]    = insp->transform[2].x.value;
            tr->scale[1]    = insp->transform[2].y.value;
            tr->scale[2]    = insp->transform[2].z.value;
            tr->dirty = true;
            any_changed = true;
        }

        gui_separator();
    }

    // ── Mesh ────────────────────────────────────────────────────────────
    rl_mesh_component *mc = mesh_get(cs, entity);
    if (mc) {
        section_header("Mesh", t, font);

        static const char *kind_items[] = {"Lit", "Unlit"};
        gui_field_begin("Kind", &fcfg);
        gui_dropdown_cfg dd_cfg = {
            .items = kind_items,
            .item_count = 2,
            .width = 100,
            .color = t->control,
            .hover_color = t->control_hover,
            .text_color = t->text,
            .font = font,
            .font_size = 12,
        };
        if (gui_dropdown(&insp->mesh_kind, &dd_cfg)) {
            rl_mesh_component before = *mc;
            mc->kind = (rl_frame_mesh_kind)insp->mesh_kind.selected;
            if (insp->undo) {
                ed_undo_entry entry = {.action = ED_UNDO_MESH, .entity = entity};
                entry.mesh.before = before;
                entry.mesh.after = *mc;
                ed_undo_push(insp->undo, &entry);
            }
            any_changed = true;
        }
        gui_field_end();

        gui_field_begin("Wireframe", &fcfg);
        if (gui_checkbox(&insp->wireframe, nullptr)) {
            rl_mesh_component before = *mc;
            mc->wireframe = insp->wireframe;
            if (insp->undo) {
                ed_undo_entry entry = {.action = ED_UNDO_MESH, .entity = entity};
                entry.mesh.before = before;
                entry.mesh.after = *mc;
                ed_undo_push(insp->undo, &entry);
            }
            any_changed = true;
        }
        gui_field_end();

        // Material sub-section
        section_header("Material", t, font);

        gui_number_input_cfg mat_v3_cfg = {
            .step = 0.005f, .min = 0, .max = 1, .format = "%.3f", .width = 50, .height = 18,
        };
        gui_field_begin("Specular", &fcfg);
        b8 spec_changed = vec3_row(&insp->mat_specular, &mat_v3_cfg, dt);
        gui_field_end();

        gui_number_input_cfg shin_cfg = {
            .step = 0.5f, .min = 0, .max = 256, .format = "%.1f", .width = 55, .height = 18,
        };
        gui_field_begin("Shininess", &fcfg);
        b8 shin_changed = gui_number_input(&insp->mat_shininess, &shin_cfg, dt);
        gui_field_end();

        if (spec_changed || shin_changed) {
            mc->material.specular[0] = insp->mat_specular.x.value;
            mc->material.specular[1] = insp->mat_specular.y.value;
            mc->material.specular[2] = insp->mat_specular.z.value;
            mc->material.shininess   = insp->mat_shininess.value;
            any_changed = true;
        }

        gui_separator();
    }

    // ── Behavior ────────────────────────────────────────────────────────
    rl_behavior_component *bc = behavior_comp_get(cs, entity);
    if (bc) {
        section_header("Behavior", t, font);
        gui_text(bc->name, &dim_text);
        gui_separator();
    }

    // ── Light ───────────────────────────────────────────────────────────
    rl_light_component *lc = light_get(cs, entity);
    if (lc) {
        section_header("Light", t, font);

        gui_number_input_cfg light_cfg = {
            .step = 0.005f, .min = 0, .max = 1, .format = "%.3f", .width = 50, .height = 18,
        };

        static const char *light_labels[] = {"Ambient", "Diffuse", "Specular"};
        for (u32 i = 0; i < 3; i++) {
            gui_field_begin(light_labels[i], &fcfg);
            b8 changed = vec3_row(&insp->light[i], &light_cfg, dt);
            gui_field_end();

            if (changed) {
                f32 *dst = (i == 0) ? lc->ambient : (i == 1) ? lc->diffuse : lc->specular;
                dst[0] = insp->light[i].x.value;
                dst[1] = insp->light[i].y.value;
                dst[2] = insp->light[i].z.value;
                any_changed = true;
            }

        }
    }

    // ── Drag lifecycle tracking for undo coalescing ────────────────────
    if (insp->undo) {
        b8 is_transform_dragging = false;
        if (tr) {
            for (u32 i = 0; i < 3; i++) {
                is_transform_dragging |= insp->transform[i].x.dragging;
                is_transform_dragging |= insp->transform[i].y.dragging;
                is_transform_dragging |= insp->transform[i].z.dragging;
            }
        }

        b8 is_mesh_dragging = false;
        if (mc) {
            is_mesh_dragging = insp->mat_specular.x.dragging ||
                               insp->mat_specular.y.dragging ||
                               insp->mat_specular.z.dragging ||
                               insp->mat_shininess.dragging;
        }

        b8 is_light_dragging = false;
        if (lc) {
            for (u32 i = 0; i < 3; i++) {
                is_light_dragging |= insp->light[i].x.dragging;
                is_light_dragging |= insp->light[i].y.dragging;
                is_light_dragging |= insp->light[i].z.dragging;
            }
        }

        b8 is_any_dragging = is_transform_dragging || is_mesh_dragging || is_light_dragging;

        if (!insp->was_any_dragging && is_any_dragging) {
            // Drag started — snapshot "before" state
            insp->undo_entity = entity;
            if (is_transform_dragging) {
                insp->undo_action = ED_UNDO_TRANSFORM;
                insp->drag_start_transform = *tr;
            } else if (is_mesh_dragging) {
                insp->undo_action = ED_UNDO_MESH;
                insp->drag_start_mesh = *mc;
            } else if (is_light_dragging) {
                insp->undo_action = ED_UNDO_LIGHT;
                insp->drag_start_light = *lc;
            }
        }

        if (insp->was_any_dragging && !is_any_dragging) {
            // Drag ended — push undo entry with before + current as after
            ed_undo_entry entry = {.action = (ED_UNDO_ACTION)insp->undo_action,
                                   .entity = insp->undo_entity};
            switch (insp->undo_action) {
            case ED_UNDO_TRANSFORM:
                entry.transform.before = insp->drag_start_transform;
                entry.transform.after = *transform_get(cs, insp->undo_entity);
                break;
            case ED_UNDO_MESH:
                entry.mesh.before = insp->drag_start_mesh;
                entry.mesh.after = *mesh_get(cs, insp->undo_entity);
                break;
            case ED_UNDO_LIGHT:
                entry.light.before = insp->drag_start_light;
                entry.light.after = *light_get(cs, insp->undo_entity);
                break;
            default: break;
            }
            ed_undo_push(insp->undo, &entry);
        }

        insp->was_any_dragging = is_any_dragging;
    }

    if (any_changed && scene_dirty) {
        *scene_dirty = true;
    }

    return any_changed;
}

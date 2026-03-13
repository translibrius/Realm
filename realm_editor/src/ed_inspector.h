#pragma once

#include "core/entity.h"
#include "core/scene.h"
#include "defines.h"
#include "gui/gui_checkbox.h"
#include "gui/gui_dropdown.h"
#include "gui/gui_number_input.h"
#include "gui/gui_text_input.h"

typedef struct ed_inspector_vec3 {
    gui_number_input_state x, y, z;
} ed_inspector_vec3;

typedef struct ed_inspector {
    gui_text_input_state name_input;

    // Transform: position[0], rotation[1], scale[2]
    ed_inspector_vec3 transform[3];

    // Mesh
    gui_dropdown_state mesh_kind;
    b8 wireframe;

    // Material (inside mesh component)
    ed_inspector_vec3 mat_specular;
    gui_number_input_state mat_shininess;

    // Light: ambient[0], diffuse[1], specular[2]
    ed_inspector_vec3 light[3];

    // Focus tracking
    gui_number_input_state *focused_input;
    b8 name_focused;

    // Track bound entity to detect selection changes
    u32 bound_entity_idx;
} ed_inspector;

void ed_inspector_init(ed_inspector *insp);
void ed_inspector_bind(ed_inspector *insp, rl_scene *scene, rl_entity entity);
b8   ed_inspector_render(ed_inspector *insp, rl_scene *scene, rl_entity entity,
                         b8 *scene_dirty, f32 dt);

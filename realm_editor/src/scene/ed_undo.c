#include "scene/ed_undo.h"

#include "core/component.h"
#include "core/logger.h"

#include <string.h>

#define MASK (ED_UNDO_RING_SIZE - 1)

static void apply_component(rl_scene *scene, const ed_undo_entry *e, b8 use_before) {
    rl_component_store *cs = &scene->components;

    switch (e->action) {
    case ED_UNDO_TRANSFORM: {
        rl_transform *t = transform_get(cs, e->entity);
        if (t) {
            *t = use_before ? e->transform.before : e->transform.after;
            t->dirty = true;
        }
    } break;

    case ED_UNDO_MESH: {
        rl_mesh_component *m = mesh_get(cs, e->entity);
        if (m) *m = use_before ? e->mesh.before : e->mesh.after;
    } break;

    case ED_UNDO_LIGHT: {
        rl_light_component *l = light_get(cs, e->entity);
        if (l) *l = use_before ? e->light.before : e->light.after;
    } break;

    case ED_UNDO_NAME: {
        rl_name_component *n = name_get(cs, e->entity);
        if (n) *n = use_before ? e->name.before : e->name.after;
    } break;

    case ED_UNDO_CAMERA: {
        rl_camera_component *c = camera_comp_get(cs, e->entity);
        if (c) *c = use_before ? e->camera.before : e->camera.after;
    } break;

    case ED_UNDO_CREATE_ENTITY:
        if (use_before) {
            // Undo create → destroy the entity
            scene_entity_destroy(scene, e->entity);
        }
        // Redo create would need recreation — not yet wired
        break;

    case ED_UNDO_DESTROY_ENTITY:
        if (use_before) {
            // Undo destroy → recreate — not yet wired
        } else {
            scene_entity_destroy(scene, e->entity);
        }
        break;
    }
}

void ed_undo_init(ed_undo_stack *stack) {
    memset(stack, 0, sizeof(*stack));
}

void ed_undo_push(ed_undo_stack *stack, const ed_undo_entry *entry) {
    stack->entries[stack->head & MASK] = *entry;
    stack->head++;
    if (stack->count < ED_UNDO_RING_SIZE) stack->count++;
    stack->redo_count = 0;
}

b8 ed_undo_perform(ed_undo_stack *stack, rl_scene *scene) {
    if (stack->count == 0) return false;

    stack->head--;
    stack->count--;
    stack->redo_count++;

    apply_component(scene, &stack->entries[stack->head & MASK], true);
    return true;
}

b8 ed_undo_redo(ed_undo_stack *stack, rl_scene *scene) {
    if (stack->redo_count == 0) return false;

    apply_component(scene, &stack->entries[stack->head & MASK], false);

    stack->head++;
    stack->count++;
    stack->redo_count--;
    return true;
}

void ed_undo_clear(ed_undo_stack *stack) {
    stack->head = 0;
    stack->count = 0;
    stack->redo_count = 0;
}

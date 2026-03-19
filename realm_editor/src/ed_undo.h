#pragma once

#include "core/component.h"
#include "core/entity.h"
#include "core/scene.h"
#include "defines.h"

#define ED_UNDO_RING_SIZE 256

typedef enum ED_UNDO_ACTION {
    ED_UNDO_TRANSFORM,
    ED_UNDO_MESH,
    ED_UNDO_LIGHT,
    ED_UNDO_NAME,
    ED_UNDO_CAMERA,
    ED_UNDO_CREATE_ENTITY,
    ED_UNDO_DESTROY_ENTITY,
} ED_UNDO_ACTION;

typedef struct ed_entity_snapshot {
    rl_transform        transform;    b8 has_transform;
    rl_mesh_component   mesh;         b8 has_mesh;
    rl_light_component  light;        b8 has_light;
    rl_name_component   name;         b8 has_name;
    rl_camera_component camera;       b8 has_camera;
} ed_entity_snapshot;

typedef struct ed_undo_entry {
    ED_UNDO_ACTION action;
    rl_entity entity;
    union {
        struct { rl_transform       before, after; } transform;
        struct { rl_mesh_component  before, after; } mesh;
        struct { rl_light_component before, after; } light;
        struct { rl_name_component   before, after; } name;
        struct { rl_camera_component before, after; } camera;
        struct { ed_entity_snapshot snapshot; }       entity_op;
    };
} ed_undo_entry;

typedef struct ed_undo_stack {
    ed_undo_entry entries[ED_UNDO_RING_SIZE];
    u32 head;        // next write position
    u32 count;       // valid undo entries behind head
    u32 redo_count;  // valid redo entries ahead of head
} ed_undo_stack;

void ed_undo_init(ed_undo_stack *stack);
void ed_undo_push(ed_undo_stack *stack, const ed_undo_entry *entry);
b8   ed_undo_perform(ed_undo_stack *stack, rl_scene *scene);
b8   ed_undo_redo(ed_undo_stack *stack, rl_scene *scene);
void ed_undo_clear(ed_undo_stack *stack);

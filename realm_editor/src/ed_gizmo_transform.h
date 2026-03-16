#pragma once

#include "core/component.h"
#include "core/entity.h"
#include "core/scene.h"
#include "defines.h"
#include "math/ray.h"
#include "renderer/frame_data.h"

typedef enum ED_GIZMO_MODE {
    ED_GIZMO_TRANSLATE,
    ED_GIZMO_ROTATE,
    ED_GIZMO_SCALE,
} ED_GIZMO_MODE;

typedef enum ED_GIZMO_AXIS {
    ED_GIZMO_AXIS_NONE = 0,
    ED_GIZMO_AXIS_X    = 1,
    ED_GIZMO_AXIS_Y    = 2,
    ED_GIZMO_AXIS_Z    = 3,
} ED_GIZMO_AXIS;

typedef struct ed_gizmo_transform {
    ED_GIZMO_MODE mode;
    b8            dragging;
    ED_GIZMO_AXIS drag_axis;
    f32           drag_start_axis_value;
    rl_transform  drag_start_transform;
    rl_entity     drag_entity;
} ed_gizmo_transform;

void          ed_gizmo_transform_init(ed_gizmo_transform *g);
void          ed_gizmo_transform_build(ed_gizmo_transform *g, rl_scene *scene,
                                        rl_entity selected, rl_frame_data *frame);
ED_GIZMO_AXIS ed_gizmo_transform_pick(ed_gizmo_transform *g, rl_scene *scene,
                                        rl_entity selected, const rl_ray *ray);
void          ed_gizmo_transform_drag_begin(ed_gizmo_transform *g, rl_scene *scene,
                                             rl_entity entity, ED_GIZMO_AXIS axis,
                                             const rl_ray *ray);
void          ed_gizmo_transform_drag_update(ed_gizmo_transform *g, rl_scene *scene,
                                              const rl_ray *ray);
b8            ed_gizmo_transform_drag_end(ed_gizmo_transform *g);

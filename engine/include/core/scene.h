#pragma once

#include "core/component.h"
#include "core/entity.h"
#include "defines.h"
#include "memory/arena.h"
#include "renderer/frame_data.h"

#define RL_SCENE_NAME_MAX    64
#define RL_SCENE_DEFAULT_CAP 1024

typedef struct rl_scene {
    char                name[RL_SCENE_NAME_MAX];
    rl_arena           *arena;
    rl_entity_store     entities;
    rl_component_store  components;
} rl_scene;

REALM_API rl_scene *scene_create(const char *name);
REALM_API void      scene_destroy(rl_scene *scene);
REALM_API rl_entity scene_entity_create(rl_scene *scene, const char *name);
REALM_API void      scene_entity_destroy(rl_scene *scene, rl_entity e);
REALM_API b8        scene_entity_is_alive(const rl_scene *scene, rl_entity e);
REALM_API rl_entity scene_entity_find(const rl_scene *scene, const char *name);
REALM_API rl_entity scene_get_main_camera(const rl_scene *scene);
REALM_API void      scene_build_frame_data(rl_scene *scene, const rl_frame_camera *camera, rl_frame_data *out);

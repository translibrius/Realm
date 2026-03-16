#pragma once

#include "core/entity.h"
#include "defines.h"

typedef struct rl_scene rl_scene;

typedef void (*behavior_fn)(rl_scene *scene, rl_entity entity, f32 dt);

#define RL_BEHAVIOR_NAME_MAX     64
#define RL_BEHAVIOR_REGISTRY_MAX 64

REALM_API void        behavior_registry_init(void);
REALM_API void        behavior_registry_clear(void);
REALM_API void        behavior_register(const char *name, behavior_fn fn);
REALM_API behavior_fn behavior_find(const char *name);

REALM_API void behavior_update_all(rl_scene *scene, f32 dt);

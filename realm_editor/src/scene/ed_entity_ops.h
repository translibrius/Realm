#pragma once

#include "core/entity.h"
#include "defines.h"

typedef struct rl_scene rl_scene;

// Creates an empty entity with a default transform (identity position, unit scale).
rl_entity ed_entity_create_empty(rl_scene *scene, const char *name);

// Creates a light entity with default properties.
rl_entity ed_entity_create_light(rl_scene *scene);

// Creates a cube entity with default mesh + material.
rl_entity ed_entity_create_cube(rl_scene *scene);

// Duplicates an existing entity (copies name, transform).
rl_entity ed_entity_duplicate(rl_scene *scene, rl_entity source);

// Destroys an entity.
void ed_entity_delete(rl_scene *scene, rl_entity entity);

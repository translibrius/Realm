#include "core/behavior.h"

#include "core/component.h"
#include "core/logger.h"
#include "core/scene.h"
#include "util/str.h"

typedef struct behavior_entry {
    char        name[RL_BEHAVIOR_NAME_MAX];
    behavior_fn fn;
} behavior_entry;

static behavior_entry s_entries[RL_BEHAVIOR_REGISTRY_MAX];
static u32            s_count;

static char s_warned_names[RL_BEHAVIOR_REGISTRY_MAX][RL_BEHAVIOR_NAME_MAX];
static u32  s_warned_count;

void behavior_registry_init(void) {
    s_count = 0;
    s_warned_count = 0;
    for (u32 i = 0; i < RL_BEHAVIOR_REGISTRY_MAX; i++) {
        s_entries[i].name[0] = '\0';
        s_entries[i].fn = nullptr;
    }
}

void behavior_registry_clear(void) {
    behavior_registry_init();
}

void behavior_register(const char *name, behavior_fn fn) {
    if (!name || !fn) return;

    // Overwrite if already registered
    for (u32 i = 0; i < s_count; i++) {
        if (cstr_eq(s_entries[i].name, name)) {
            s_entries[i].fn = fn;
            return;
        }
    }

    if (s_count >= RL_BEHAVIOR_REGISTRY_MAX) {
        RL_ERROR("behavior_register: registry full (%d max)", RL_BEHAVIOR_REGISTRY_MAX);
        return;
    }

    cstr_copy(s_entries[s_count].name, RL_BEHAVIOR_NAME_MAX, name);
    s_entries[s_count].fn = fn;
    s_count++;
}

behavior_fn behavior_find(const char *name) {
    if (!name) return nullptr;

    for (u32 i = 0; i < s_count; i++) {
        if (cstr_eq(s_entries[i].name, name)) {
            return s_entries[i].fn;
        }
    }
    return nullptr;
}

static b8 already_warned(const char *name) {
    for (u32 i = 0; i < s_warned_count; i++) {
        if (cstr_eq(s_warned_names[i], name)) return true;
    }
    return false;
}

static void mark_warned(const char *name) {
    if (s_warned_count >= RL_BEHAVIOR_REGISTRY_MAX) return;
    cstr_copy(s_warned_names[s_warned_count], RL_BEHAVIOR_NAME_MAX, name);
    s_warned_count++;
}

void behavior_update_all(rl_scene *scene, f32 dt) {
    if (!scene) return;

    rl_entity_store    *es = &scene->entities;
    rl_component_store *cs = &scene->components;

    for (u32 i = 1; i < es->high_water; i++) {
        if (!es->alive[i]) continue;
        if (!cs->has_behavior[i]) continue;

        const char *name = cs->behaviors[i].name;
        behavior_fn fn = behavior_find(name);
        if (fn) {
            rl_entity e = rl_entity_pack(i, es->generation[i]);
            fn(scene, e, dt);
        } else if (!already_warned(name)) {
            RL_DEBUG("behavior_update_all: unknown behavior '%s' on entity %u", name, i);
            mark_warned(name);
        }
    }
}

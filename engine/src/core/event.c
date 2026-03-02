#include "core/event.h"
#include "core/logger.h"
#include "memory/arena.h"
#include "util/assert.h"

#define MAX_EVENTS 100

typedef struct event_system_state {
    rl_arena events_arena;
    rl_event *registered_events[MAX_EVENTS];
    u16 registered_count;
    b8 initialized;
} event_system_state;

static event_system_state *state;

u64 event_system_size() {
    return sizeof(event_system_state);
}

b8 event_system_start(void *memory) {
    RL_ASSERT_MSG(!state, "Event system already started!");
    state = memory;
    rl_arena_init(&state->events_arena, MiB(24), MiB(5), MEM_SUBSYSTEM_EVENT);
    state->initialized = true;
    state->registered_count = 0;

    RL_INFO("Event system started!");
    return true;
}
void event_system_shutdown() {
    rl_arena_deinit(&state->events_arena);
    RL_INFO("Event system shutdown...");
}

void event_fire(EVENT_TYPE type, void *event_data) {
    for (u16 i = 0; i < state->registered_count; i++) {
        rl_event *event = state->registered_events[i];
        if (!event) {
            RL_WARN("Tried to fire an unregistered event");
            continue;
        }

        if (event->type != type) {
            continue;
        }

        if (!event->event_callback) {
            RL_WARN("Tried to fire an unregistered callback");
            continue;
        }

        if (event->event_callback(event_data, event->user_data)) {
            return; // Event consumed by callback
        }
    }
}

void event_register(EVENT_TYPE type, b8 (*callback)(void *data, void *user_data), void *user_data) {
    RL_ASSERT(state->initialized);
    RL_ASSERT(state->registered_count < MAX_EVENTS);
    RL_ASSERT(callback);

    rl_event *event = rl_arena_push(&state->events_arena, sizeof(rl_event), alignof(rl_event));
    event->event_callback = callback;
    event->type = type;
    event->user_data = user_data;

    state->registered_events[state->registered_count] = event;
    state->registered_count++;
}

void event_unregister(EVENT_TYPE type, b8 (*callback)(void *data, void *user_data), void *user_data) {
    for (u16 i = 0; i < state->registered_count; i++) {
        rl_event *event = state->registered_events[i];
        if (!event) continue;

        if (event->type == type && event->event_callback == callback && event->user_data == user_data) {
            // Shift remaining entries down
            for (u16 j = i; j < state->registered_count - 1; j++) {
                state->registered_events[j] = state->registered_events[j + 1];
            }
            state->registered_events[state->registered_count - 1] = nullptr;
            state->registered_count--;
            return;
        }
    }

    RL_WARN("event_unregister: no matching callback found for event type %d", type);
}

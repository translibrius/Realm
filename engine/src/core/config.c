#include "core/config.h"

#include "core/event.h"
#include "core/logger.h"
#include "memory/arena.h"
#include "platform/io/file_io.h"
#include "util/str.h"
#include "util/toml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Enum string tables ---

typedef struct enum_entry {
    const char *name;
    i32 value;
} enum_entry;

static const enum_entry window_mode_names[] = {
    {"windowed",   WINDOW_MODE_WINDOWED  },
    {"borderless", WINDOW_MODE_BORDERLESS},
    {"fullscreen", WINDOW_MODE_FULLSCREEN},
    {nullptr,      0                     },
};

static const enum_entry backend_names[] = {
    {"opengl", BACKEND_OPENGL},
    {"vulkan", BACKEND_VULKAN},
    {nullptr,  0             },
};

static const enum_entry log_level_names[] = {
    {"info",  LOG_INFO },
    {"debug", LOG_DEBUG},
    {"trace", LOG_TRACE},
    {"warn",  LOG_WARN },
    {"error", LOG_ERROR},
    {"fatal", LOG_FATAL},
    {nullptr, 0        },
};

static const enum_entry msaa_names[] = {
    {"off", MSAA_OFF},
    {"2x",  MSAA_2X },
    {"4x",  MSAA_4X },
    {"8x",  MSAA_8X },
    {nullptr, 0     },
};

static b8 enum_from_str(const enum_entry *table, const char *str, i32 *out) {
    for (const enum_entry *e = table; e->name; e++) {
        if (cstr_cmp_nocase(e->name, str) == 0) {
            *out = e->value;
            return true;
        }
    }
    return false;
}

static const char *enum_to_str(const enum_entry *table, i32 value) {
    for (const enum_entry *e = table; e->name; e++) {
        if (e->value == value) {
            return e->name;
        }
    }
    return "unknown";
}

// --- Config state ---

static const f64 CONFIG_FLUSH_INTERVAL = 5.0;

typedef struct config_state {
    rl_config config;
    b8 dirty;
    f64 time_since_flush;
    platform_window *tracked_window;
    char filename[64];
} config_state;

static config_state *state;

// --- Defaults ---

rl_config config_defaults(void) {
    return (rl_config){
        .window_width = 500,
        .window_height = 500,
        .window_x = 0,
        .window_y = 0,
        .window_mode = WINDOW_MODE_WINDOWED,
        .renderer_backend = BACKEND_OPENGL,
        .vsync = false,
        .msaa = MSAA_OFF,
        .log_level = LOG_TRACE,
        .fov = 90.0f,
        .mouse_sensitivity = 0.1f,
    };
}

// --- Load / Save ---

static b8 config_load(void) {
    toml_table *t = toml_parse_file(state->filename);
    if (!t) {
        RL_INFO("No config file found, using defaults");
        return false;
    }

    rl_config *cfg = &state->config;
    cfg->window_width      = toml_get_int(t, "window", "width", cfg->window_width);
    cfg->window_height     = toml_get_int(t, "window", "height", cfg->window_height);
    cfg->window_x          = toml_get_int(t, "window", "x", cfg->window_x);
    cfg->window_y          = toml_get_int(t, "window", "y", cfg->window_y);
    cfg->vsync             = toml_get_bool(t, "renderer", "vsync", cfg->vsync);
    cfg->fov               = toml_get_float(t, "camera", "fov", cfg->fov);
    cfg->mouse_sensitivity = toml_get_float(t, "camera", "sensitivity", cfg->mouse_sensitivity);

    const char *s;
    i32 v;
    s = toml_get_string(t, "window", "mode", nullptr);
    if (s && enum_from_str(window_mode_names, s, &v)) cfg->window_mode = (PLATFORM_WINDOW_MODE)v;

    s = toml_get_string(t, "renderer", "backend", nullptr);
    if (s && enum_from_str(backend_names, s, &v)) cfg->renderer_backend = (RENDERER_BACKEND)v;

    s = toml_get_string(t, "engine", "log_level", nullptr);
    if (s && enum_from_str(log_level_names, s, &v)) cfg->log_level = (LOG_LEVEL)v;

    s = toml_get_string(t, "renderer", "msaa", nullptr);
    if (s && enum_from_str(msaa_names, s, &v)) cfg->msaa = (MSAA_SAMPLES)v;

    toml_free(t);

    state->config.loaded = true;
    RL_INFO("Config loaded from '%s'", state->filename);
    return true;
}

b8 config_save(void) {
    if (!state) return false;

    const rl_config *cfg = &state->config;

    char buf[1024];
    i32 len = snprintf(buf, sizeof(buf),
        "# Realm engine configuration\n"
        "\n"
        "[window]\n"
        "width = %d\n"
        "height = %d\n"
        "x = %d\n"
        "y = %d\n"
        "mode = \"%s\"\n"
        "\n"
        "[renderer]\n"
        "backend = \"%s\"\n"
        "vsync = %s\n"
        "msaa = \"%s\"\n"
        "\n"
        "[engine]\n"
        "log_level = \"%s\"\n"
        "\n"
        "[camera]\n"
        "fov = %.1f\n"
        "sensitivity = %.3f\n",
        cfg->window_width,
        cfg->window_height,
        cfg->window_x,
        cfg->window_y,
        enum_to_str(window_mode_names, (i32)cfg->window_mode),
        enum_to_str(backend_names, (i32)cfg->renderer_backend),
        cfg->vsync ? "true" : "false",
        enum_to_str(msaa_names, (i32)cfg->msaa),
        enum_to_str(log_level_names, (i32)cfg->log_level),
        (f64)cfg->fov,
        (f64)cfg->mouse_sensitivity);

    if (len < 0 || len >= (i32)sizeof(buf)) {
        RL_ERROR("Config save: buffer overflow");
        return false;
    }

    if (!platform_file_write_all(state->filename, buf, (u64)len)) {
        RL_ERROR("Failed to write config file '%s'", state->filename);
        return false;
    }

    state->dirty = false;
    state->time_since_flush = 0.0;
    RL_DEBUG("Config saved to '%s'", state->filename);
    return true;
}

// --- Event callback ---

static b8 on_window_resize(void *event, void *user_data) {
    (void)user_data;
    if (!state) return false;

    platform_window *window = event;

    // Only persist position/size for the tracked (main) window.
    if (window != state->tracked_window) return false;

    state->config.window_x = window->settings.x;
    state->config.window_y = window->settings.y;
    state->config.window_width = window->settings.width;
    state->config.window_height = window->settings.height;
    state->config.window_mode = window->settings.window_mode;
    config_mark_dirty();

    return false; // Don't consume — realm's handler needs it too
}

// --- Subsystem lifecycle ---

u64 config_system_size(void) {
    return sizeof(config_state);
}

b8 config_system_start(void *memory, const char *filename) {
    state = (config_state *)memory;
    state->config = config_defaults();
    state->dirty = false;
    state->time_since_flush = 0.0;

    const char *fn = (filename && filename[0]) ? filename : RL_CONFIG_FILENAME_DEFAULT;
    u64 len = cstr_len(fn);
    if (len >= sizeof(state->filename)) len = sizeof(state->filename) - 1;
    memcpy(state->filename, fn, len);
    state->filename[len] = '\0';

    config_load();

    event_register(EVENT_WINDOW_RESIZE, on_window_resize, nullptr);

    RL_INFO("Config system initialized");
    return true;
}

void config_system_shutdown(void) {
    if (!state) return;

    config_save();
    RL_INFO("Config system shut down");
    state = nullptr;
}

// --- Public API ---

platform_window_settings config_to_window_settings(const rl_config *cfg, const char *title) {
    return (platform_window_settings){
        .title = title,
        .x = cfg->window_x,
        .y = cfg->window_y,
        .width = cfg->window_width,
        .height = cfg->window_height,
        .start_center = !cfg->loaded,
        .window_flags = WINDOW_FLAG_DEFAULT,
        .window_mode = cfg->window_mode,
    };
}

rl_config *config_get(void) {
    return state ? &state->config : nullptr;
}

void config_set_vsync(b8 value) {
    if (!state) return;
    if (state->config.vsync == value) return;
    state->config.vsync = value;
    config_mark_dirty();
    e_config_changed_payload payload = { .key = "vsync" };
    event_fire(EVENT_CONFIG_CHANGED, &payload);
}

void config_set_fov(f32 value) {
    if (!state) return;
    if (state->config.fov == value) return;
    state->config.fov = value;
    config_mark_dirty();
    e_config_changed_payload payload = { .key = "fov" };
    event_fire(EVENT_CONFIG_CHANGED, &payload);
}

void config_set_mouse_sensitivity(f32 value) {
    if (!state) return;
    if (state->config.mouse_sensitivity == value) return;
    state->config.mouse_sensitivity = value;
    config_mark_dirty();
    e_config_changed_payload payload = { .key = "mouse_sensitivity" };
    event_fire(EVENT_CONFIG_CHANGED, &payload);
}

void config_set_log_level(LOG_LEVEL level) {
    if (!state) return;
    if (state->config.log_level == level) return;
    state->config.log_level = level;
    logger_set_level(level);
    config_mark_dirty();
    e_config_changed_payload payload = { .key = "log_level" };
    event_fire(EVENT_CONFIG_CHANGED, &payload);
}

void config_set_msaa(MSAA_SAMPLES value) {
    if (!state) return;
    if (state->config.msaa == value) return;
    state->config.msaa = value;
    config_mark_dirty();
    e_config_changed_payload payload = { .key = "msaa" };
    event_fire(EVENT_CONFIG_CHANGED, &payload);
}

void config_mark_dirty(void) {
    if (state) {
        state->dirty = true;
    }
}

void config_track_window(platform_window *window) {
    if (state) {
        state->tracked_window = window;
    }
}

void config_flush_if_dirty(f64 dt) {
    if (!state || !state->dirty) return;

    state->time_since_flush += dt;
    if (state->time_since_flush >= CONFIG_FLUSH_INTERVAL) {
        config_save();
    }
}

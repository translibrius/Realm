#include "core/config.h"

#include "core/event.h"
#include "core/logger.h"
#include "memory/arena.h"
#include "platform/io/file_io.h"

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

static b8 enum_from_str(const enum_entry *table, const char *str, i32 *out) {
    for (const enum_entry *e = table; e->name; e++) {
        // Case-insensitive compare
        const char *a = e->name;
        const char *b = str;
        b8 match = true;
        while (*a && *b) {
            char ca = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
            char cb = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
            if (ca != cb) {
                match = false;
                break;
            }
            a++;
            b++;
        }
        if (match && *a == '\0' && *b == '\0') {
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
        .log_level = LOG_TRACE,
        .fov = 90.0f,
        .mouse_sensitivity = 0.1f,
    };
}

// --- TOML parsing helpers ---

static char *trim_whitespace(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    return s;
}

// Current table name tracked across lines during parsing
static char current_table[32];

static b8 config_parse_table_header(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line != '[') return false;
    line++;

    const char *end = strchr(line, ']');
    if (!end) return false;

    u32 len = (u32)(end - line);
    if (len >= sizeof(current_table)) return false;
    memcpy(current_table, line, len);
    current_table[len] = '\0';
    return true;
}

static void config_parse_line(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '#' || *line == '\n' || *line == '\r') {
        return;
    }

    // Table header
    if (*line == '[') {
        config_parse_table_header(line);
        return;
    }

    // Find '='
    const char *eq = strchr(line, '=');
    if (!eq) return;

    // Extract key (before '=')
    char key[64] = {0};
    u32 key_len = (u32)(eq - line);
    if (key_len >= sizeof(key)) return;
    memcpy(key, line, key_len);
    key[key_len] = '\0';

    // Trim key
    char *k = key;
    while (*k == ' ' || *k == '\t') k++;
    char *kend = k + strlen(k) - 1;
    while (kend > k && (*kend == ' ' || *kend == '\t')) {
        *kend = '\0';
        kend--;
    }

    // Build fully-qualified key: "table.key"
    char fqk[96] = {0};
    if (current_table[0]) {
        snprintf(fqk, sizeof(fqk), "%s.%s", current_table, k);
    } else {
        snprintf(fqk, sizeof(fqk), "%s", k);
    }

    // Extract value (after '=')
    char val_buf[128] = {0};
    const char *v = eq + 1;
    while (*v == ' ' || *v == '\t') v++;

    u32 vlen = 0;
    while (v[vlen] && v[vlen] != '\n' && v[vlen] != '\r' && v[vlen] != '#' && vlen < sizeof(val_buf) - 1) {
        vlen++;
    }
    memcpy(val_buf, v, vlen);
    val_buf[vlen] = '\0';

    // Trim trailing whitespace from value
    char *val = trim_whitespace(val_buf);

    // Determine value type and apply
    rl_config *cfg = &state->config;

    // Check if quoted string (enum)
    u32 val_len = (u32)strlen(val);
    if (val_len >= 2 && val[0] == '"' && val[val_len - 1] == '"') {
        // Strip quotes
        val[val_len - 1] = '\0';
        char *str_val = val + 1;

        i32 enum_val;
        if (strcmp(fqk, "window.mode") == 0) {
            if (enum_from_str(window_mode_names, str_val, &enum_val)) {
                cfg->window_mode = (PLATFORM_WINDOW_MODE)enum_val;
            } else {
                RL_WARN("Config: unknown window.mode '%s', using default", str_val);
            }
        } else if (strcmp(fqk, "renderer.backend") == 0) {
            if (enum_from_str(backend_names, str_val, &enum_val)) {
                cfg->renderer_backend = (RENDERER_BACKEND)enum_val;
            } else {
                RL_WARN("Config: unknown renderer.backend '%s', using default", str_val);
            }
        } else if (strcmp(fqk, "engine.log_level") == 0) {
            if (enum_from_str(log_level_names, str_val, &enum_val)) {
                cfg->log_level = (LOG_LEVEL)enum_val;
            } else {
                RL_WARN("Config: unknown engine.log_level '%s', using default", str_val);
            }
        }
        return;
    }

    // Boolean
    if (strcmp(val, "true") == 0 || strcmp(val, "false") == 0) {
        b8 bool_val = strcmp(val, "true") == 0;
        if (strcmp(fqk, "renderer.vsync") == 0) {
            cfg->vsync = bool_val;
        }
        return;
    }

    // Float (contains '.')
    char *endptr = nullptr;
    if (strchr(val, '.')) {
        f64 float_val = strtod(val, &endptr);
        if (endptr != val && *endptr == '\0') {
            if (strcmp(fqk, "camera.fov") == 0)              cfg->fov = (f32)float_val;
            else if (strcmp(fqk, "camera.sensitivity") == 0) cfg->mouse_sensitivity = (f32)float_val;
        }
        return;
    }

    // Integer
    i64 int_val = strtol(val, &endptr, 10);
    if (endptr != val && *endptr == '\0') {
        if (strcmp(fqk, "window.width") == 0)       cfg->window_width = (i32)int_val;
        else if (strcmp(fqk, "window.height") == 0)  cfg->window_height = (i32)int_val;
        else if (strcmp(fqk, "window.x") == 0)       cfg->window_x = (i32)int_val;
        else if (strcmp(fqk, "window.y") == 0)       cfg->window_y = (i32)int_val;
    }
}

// --- Load / Save ---

static b8 config_load(void) {
    if (!platform_file_exists(RL_CONFIG_FILENAME)) {
        RL_INFO("No config file found, using defaults");
        return false;
    }

    rl_file file = {0};
    if (!platform_file_open(RL_CONFIG_FILENAME, P_FILE_READ, &file)) {
        RL_WARN("Failed to open config file, using defaults");
        return false;
    }

    if (!platform_file_read_all(&file)) {
        RL_WARN("Failed to read config file, using defaults");
        platform_file_close(&file);
        return false;
    }

    // Parse line by line (work on a mutable copy since buf may not be null-terminated)
    rl_temp_arena scratch = rl_arena_scratch_get();
    u64 buf_size = file.buf_len + 1;
    char *text = rl_arena_push(scratch.arena, buf_size, false);
    memcpy(text, file.buf, file.buf_len);
    text[file.buf_len] = '\0';

    current_table[0] = '\0';
    char *line = text;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) {
            *next = '\0';
            next++;
        }
        config_parse_line(line);
        line = next;
    }

    arena_scratch_release(scratch);
    platform_file_close(&file);

    state->config.loaded = true;
    RL_INFO("Config loaded from '%s'", RL_CONFIG_FILENAME);
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
        enum_to_str(log_level_names, (i32)cfg->log_level),
        (f64)cfg->fov,
        (f64)cfg->mouse_sensitivity);

    if (len < 0 || len >= (i32)sizeof(buf)) {
        RL_ERROR("Config save: buffer overflow");
        return false;
    }

    if (!platform_file_write_all(RL_CONFIG_FILENAME, buf, (u64)len)) {
        RL_ERROR("Failed to write config file '%s'", RL_CONFIG_FILENAME);
        return false;
    }

    state->dirty = false;
    state->time_since_flush = 0.0;
    RL_DEBUG("Config saved to '%s'", RL_CONFIG_FILENAME);
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

b8 config_system_start(void *memory) {
    state = (config_state *)memory;
    state->config = config_defaults();
    state->dirty = false;
    state->time_since_flush = 0.0;

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

#include "util/toml.h"

#include "memory/arena.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#include <stdlib.h>
#include <string.h>

typedef struct toml_entry {
    char section[32];
    char key[64];
    char *value;
} toml_entry;

struct toml_table {
    rl_arena *arena;
    toml_entry *entries;
    u32 count;
    u32 capacity;
};

static void table_push(toml_table *t, const char *section, const char *key, const char *value) {
    if (t->count >= t->capacity) {
        u32 new_cap = t->capacity * 2;
        toml_entry *new_entries = rl_arena_push(t->arena, new_cap * sizeof(toml_entry), true);
        memcpy(new_entries, t->entries, t->count * sizeof(toml_entry));
        t->entries = new_entries;
        t->capacity = new_cap;
    }

    toml_entry *e = &t->entries[t->count++];

    u32 slen = cstr_len(section);
    if (slen >= sizeof(e->section)) slen = sizeof(e->section) - 1;
    memcpy(e->section, section, slen);
    e->section[slen] = '\0';

    u32 klen = cstr_len(key);
    if (klen >= sizeof(e->key)) klen = sizeof(e->key) - 1;
    memcpy(e->key, key, klen);
    e->key[klen] = '\0';

    u32 vlen = cstr_len(value);
    e->value = rl_arena_push(t->arena, vlen + 1, false);
    memcpy(e->value, value, vlen + 1);
}

static const toml_entry *table_find(const toml_table *t, const char *section, const char *key) {
    const char *sec = (section && section[0]) ? section : "";
    for (u32 i = 0; i < t->count; i++) {
        if (cstr_eq(t->entries[i].section, sec) && cstr_eq(t->entries[i].key, key)) {
            return &t->entries[i];
        }
    }
    return nullptr;
}

toml_table *toml_parse(const char *text, u64 len) {
    rl_arena *arena = rl_arena_create(KiB(16), KiB(4), MEM_ARENA);

    toml_table *t = rl_arena_push(arena, sizeof(toml_table), true);
    t->arena = arena;
    t->capacity = 32;
    t->entries = rl_arena_push(arena, t->capacity * sizeof(toml_entry), true);

    // Copy text to arena so we can mutate it
    char *buf = rl_arena_push(arena, len + 1, false);
    memcpy(buf, text, len);
    buf[len] = '\0';

    char current_section[32] = {0};

    char *line = buf;
    while (line && *line) {
        char *next = (char *)cstr_find_char(line, '\n');
        if (next) { *next = '\0'; next++; }

        // Skip leading whitespace
        while (*line == ' ' || *line == '\t') line++;

        // Skip empty lines and comments
        if (*line == '\0' || *line == '#' || *line == '\r') {
            line = next;
            continue;
        }

        // Section header
        if (*line == '[') {
            const char *end = cstr_find_char(line + 1, ']');
            if (end) {
                u32 slen = (u32)(end - line - 1);
                if (slen >= sizeof(current_section)) slen = sizeof(current_section) - 1;
                memcpy(current_section, line + 1, slen);
                current_section[slen] = '\0';
            }
            line = next;
            continue;
        }

        // Key = value
        char *eq = (char *)cstr_find_char(line, '=');
        if (!eq) { line = next; continue; }

        // Extract and trim key
        *eq = '\0';
        char *k = line;
        while (*k == ' ' || *k == '\t') k++;
        char *kend = k + cstr_len(k) - 1;
        while (kend > k && (*kend == ' ' || *kend == '\t')) { *kend = '\0'; kend--; }

        // Extract value
        char *v = eq + 1;
        while (*v == ' ' || *v == '\t') v++;

        // Strip \r
        u32 vlen = cstr_len(v);
        while (vlen > 0 && (v[vlen - 1] == '\r' || v[vlen - 1] == '\n')) { v[--vlen] = '\0'; }

        // Strip inline comment (but not inside quotes)
        if (v[0] != '"') {
            char *hash = (char *)cstr_find_char(v, '#');
            if (hash) {
                *hash = '\0';
                vlen = cstr_len(v);
            }
        } else {
            // For quoted strings, find closing quote first, then strip comment after
            char *closing = (char *)cstr_find_char(v + 1, '"');
            if (closing) {
                char *hash = (char *)cstr_find_char(closing + 1, '#');
                if (hash) {
                    *hash = '\0';
                    vlen = cstr_len(v);
                }
            }
        }

        // Trim trailing whitespace from value
        while (vlen > 0 && (v[vlen - 1] == ' ' || v[vlen - 1] == '\t')) { v[--vlen] = '\0'; }

        // Strip surrounding quotes
        if (vlen >= 2 && v[0] == '"' && v[vlen - 1] == '"') {
            v[vlen - 1] = '\0';
            v++;
        }

        if (k[0]) {
            table_push(t, current_section, k, v);
        }

        line = next;
    }

    return t;
}

toml_table *toml_parse_file(const char *path) {
    if (!platform_file_exists(path)) return nullptr;

    rl_file file = {0};
    if (!platform_file_open(path, P_FILE_READ, &file)) return nullptr;
    if (!platform_file_read_all(&file)) {
        platform_file_close(&file);
        return nullptr;
    }

    toml_table *t = toml_parse((const char *)file.buf, file.buf_len);
    platform_file_close(&file);
    return t;
}

void toml_free(toml_table *table) {
    if (!table) return;
    rl_arena *arena = table->arena;
    rl_arena_destroy(arena);
}

const char *toml_get_string(const toml_table *t, const char *section, const char *key, const char *fallback) {
    if (!t) return fallback;
    const toml_entry *e = table_find(t, section, key);
    return e ? e->value : fallback;
}

i32 toml_get_int(const toml_table *t, const char *section, const char *key, i32 fallback) {
    if (!t) return fallback;
    const toml_entry *e = table_find(t, section, key);
    if (!e) return fallback;
    char *end = nullptr;
    i64 v = strtol(e->value, &end, 10);
    if (end == e->value || *end != '\0') return fallback;
    return (i32)v;
}

f32 toml_get_float(const toml_table *t, const char *section, const char *key, f32 fallback) {
    if (!t) return fallback;
    const toml_entry *e = table_find(t, section, key);
    if (!e) return fallback;
    char *end = nullptr;
    f64 v = strtod(e->value, &end);
    if (end == e->value || *end != '\0') return fallback;
    return (f32)v;
}

b8 toml_get_bool(const toml_table *t, const char *section, const char *key, b8 fallback) {
    if (!t) return fallback;
    const toml_entry *e = table_find(t, section, key);
    if (!e) return fallback;
    if (cstr_eq(e->value, "true") || cstr_eq(e->value, "1")) return true;
    if (cstr_eq(e->value, "false") || cstr_eq(e->value, "0")) return false;
    return fallback;
}

b8 toml_has_key(const toml_table *t, const char *section, const char *key) {
    if (!t) return false;
    return table_find(t, section, key) != nullptr;
}

u32 toml_entry_count(const toml_table *t) {
    return t ? t->count : 0;
}

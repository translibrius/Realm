#pragma once

#include "defines.h"

typedef struct toml_table toml_table;

REALM_API toml_table *toml_parse(const char *text, u64 len);
REALM_API toml_table *toml_parse_file(const char *path);
REALM_API void        toml_free(toml_table *table);

REALM_API const char *toml_get_string(const toml_table *t, const char *section, const char *key, const char *fallback);
REALM_API i32         toml_get_int(const toml_table *t, const char *section, const char *key, i32 fallback);
REALM_API f32         toml_get_float(const toml_table *t, const char *section, const char *key, f32 fallback);
REALM_API b8          toml_get_bool(const toml_table *t, const char *section, const char *key, b8 fallback);
REALM_API b8          toml_has_key(const toml_table *t, const char *section, const char *key);
REALM_API u32         toml_entry_count(const toml_table *t);

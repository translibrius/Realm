#pragma once

#include "defines.h"
#include "memory/arena.h"
#include "memory/containers/dynamic_array.h"

typedef struct platform_dir_entry {
    const char *name; // arena-allocated filename (not full path)
    b8 is_dir;
} platform_dir_entry;

DA_DEFINE(DirEntries, platform_dir_entry);

// Single-level directory scan. Skips hidden files (. prefix) and . / .. entries.
// ext_filter: comma-separated extensions (e.g. ".jpg,.png") or NULL for all.
// Filenames are allocated on caller's arena; DA uses mem_alloc via da_append.
// Returns false if directory cannot be opened.
REALM_API b8 platform_dir_scan(const char *path,
                                const char *ext_filter,
                                rl_arena *arena,
                                DirEntries *out);

// List available drive letters (Windows only). On Unix this returns false.
// Each entry has name="X:\" and is_dir=true. Names allocated on caller's arena.
REALM_API b8 platform_list_drives(rl_arena *arena, DirEntries *out);

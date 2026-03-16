# String Utils Consolidation Plan

Replace raw C string functions with in-house `str.h` utilities across the codebase. Expand `str.h` where gaps exist.

---

## Phase 1: Expand `str.h` with missing utilities

Add to `engine/include/util/str.h` + `engine/src/util/str.c`:

```c
// Comparison
b8   cstr_eq(const char *a, const char *b);              // strcmp == 0 wrapper, null-safe
b8   cstr_eq_nocase(const char *a, const char *b);       // case-insensitive compare
b8   cstr_starts_with(const char *str, const char *pfx); // companion to cstr_ends_with

// Trimming (in-place, returns pointer into same buffer)
char *cstr_trim(char *str);                              // trim leading + trailing whitespace
char *cstr_trim_right(char *str);                        // trim trailing only
```

Tests: `tests/cases/test_str.c` — add cases for each new function.

---

## Phase 2: Migrate `toml.c` (highest density — 19 raw calls)

| Raw pattern | Replacement |
|---|---|
| `strlen(...)` × 7 | `cstr_len(...)` |
| `strcmp(a, b) == 0` × 3 | `cstr_eq(a, b)` |
| Manual whitespace trim loops | `cstr_trim()` / `cstr_trim_right()` |
| Manual quote stripping | Inline — too specific for a generic util |

`strchr` calls stay — they're low-level parsing internals, no value wrapping them.

---

## Phase 3: Migrate `config.c`

| Raw pattern | Replacement |
|---|---|
| Case-insensitive compare in `enum_from_str` | `cstr_eq_nocase()` |
| `snprintf(buf, ...)` for config serialization | `cstr_format_buf()` |
| `strlen(fn)` in `config_system_start` | `cstr_len(fn)` |
| `memcpy` + null-term for filename copy | `cstr_copy()` |

---

## Phase 4: Migrate `project.c`

| Raw pattern | Replacement |
|---|---|
| `normalize_path()` manual backslash + trailing slash | Consider `rl_path_sanitize` — but it's arena-allocated. Keep `normalize_path` but use `cstr_len` internally |
| `strlen(src)` in normalize_path | `cstr_len(src)` |
| `snprintf` for path construction | Already correct in most places via `rl_string_format` |

---

## Phase 5: Migrate `realm/src/` (hot-reload host)

**realm_app_loader.c:**
| Raw pattern | Replacement |
|---|---|
| `snprintf` × 3 for DLL path construction | `cstr_format_buf()` |
| `strncpy` × 2 | `cstr_copy()` |

**realm_app_watcher.c:**
| Raw pattern | Replacement |
|---|---|
| `strncpy` × 1 | `cstr_copy()` |

---

## Phase 6: Migrate `realm_editor/src/`

**ed_settings.c:**
| Raw pattern | Replacement |
|---|---|
| `strcmp(theme, "dark")` × 3 | `cstr_eq(theme, "dark")` |

**ed_asset_browser.c:**
| Raw pattern | Replacement |
|---|---|
| `snprintf` × 4 for path construction | `cstr_format_buf()` |

**ed_event_handler.c:**
| Raw pattern | Replacement |
|---|---|
| `snprintf` × 2 | `cstr_format_buf()` |

---

## Phase 7: Migrate engine internals

**profiler.c:**
| Raw pattern | Replacement |
|---|---|
| `strlen` × 4 for name length | `cstr_len()` |

**asset.c:**
| Raw pattern | Replacement |
|---|---|
| `strlen` × 2, `snprintf` × 4 | `cstr_len()`, `cstr_format_buf()` |

**gui_file_browser.c:**
| Raw pattern | Replacement |
|---|---|
| Manual case-insensitive sort comparison | `cstr_eq_nocase()` or a new `cstr_cmp_nocase()` returning `i32` for sorting |

**gui_number_input.c:**
| Raw pattern | Replacement |
|---|---|
| `snprintf` + `strlen` pair | `cstr_format_buf()` (returns length) |

**Platform file_scan (3 files — win32/macos/linux):**
| Raw pattern | Replacement |
|---|---|
| `strlen` × 1 each | `cstr_len()` |
| `snprintf` × 1 each | `cstr_format_buf()` |

---

## Phase 8: Vulkan backend

**vk_instance.c:**
| Raw pattern | Replacement |
|---|---|
| `strcmp` × 2 for extension matching | `cstr_eq()` |

**vk_device.c:**
| Raw pattern | Replacement |
|---|---|
| `strcmp` × 2 for device/extension matching | `cstr_eq()` |

---

## Out of scope

- `scene_io.c` — `strcpy` calls are inside yyjson serialization helpers, tied to library API
- `str.c` itself — internal use of `strlen`/`memcpy` is fine (it *implements* the wrappers)
- `strchr`/`strstr` — too low-level and context-specific to wrap generically

---

## Summary

| Phase | Files | Est. raw calls migrated |
|---|---|---|
| 1 — Expand str.h | str.h, str.c, test_str.c | (new utils) |
| 2 — toml.c | 1 | ~15 |
| 3 — config.c | 1 | ~8 |
| 4 — project.c | 1 | ~3 |
| 5 — realm host | 2 | ~6 |
| 6 — editor | 3 | ~9 |
| 7 — engine internals | 5+ | ~15 |
| 8 — vulkan | 2 | ~4 |
| **Total** | **~16 files** | **~60 calls** |

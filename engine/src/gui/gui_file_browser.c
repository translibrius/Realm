#include "gui/gui_file_browser.h"

#include "asset/asset.h"
#include "core/logger.h"
#include "engine.h"
#include "gui/gui_button.h"
#include "gui/gui_clay.h"
#include "gui/gui_panel.h"
#include "gui/gui_text.h"
#include "gui/gui_text_input.h"
#include "gui/gui_theme.h"
#include "gui/gui_scroll.h"
#include "gui/gui_window.h"
#include "memory/arena.h"
#include "memory/memory.h"
#include "platform/input.h"
#include "platform/io/file_scan.h"
#include "util/str.h"

#include <string.h>

// ---- Static helpers ----

static void fb_free_entries(gui_file_browser_state *state) {
    if (state->entries_owned) {
        for (u64 i = 0; i < state->entries.count; i++) {
            if (state->entries.items[i].name) {
                u32 len = cstr_len(state->entries.items[i].name);
                mem_free((void *)state->entries.items[i].name, len + 1, MEM_STRING);
            }
        }
    }
    da_free(&state->entries);
    da_init(&state->entries);
    state->entries_owned = false;
}

static i32 fb_compare_entries(const platform_dir_entry *a, const platform_dir_entry *b) {
    // Directories first
    if (a->is_dir && !b->is_dir) return -1;
    if (!a->is_dir && b->is_dir) return 1;
    // Alphabetical (case-insensitive)
    const char *na = a->name;
    const char *nb = b->name;
    while (*na && *nb) {
        char ca = *na, cb = *nb;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca < cb ? -1 : 1;
        na++;
        nb++;
    }
    if (*na) return 1;
    if (*nb) return -1;
    return 0;
}

static void fb_sort_entries(DirEntries *entries) {
    // Insertion sort — fine for directory listings
    for (u64 i = 1; i < entries->count; i++) {
        platform_dir_entry tmp = entries->items[i];
        u64 j = i;
        while (j > 0 && fb_compare_entries(&entries->items[j - 1], &tmp) > 0) {
            entries->items[j] = entries->items[j - 1];
            j--;
        }
        entries->items[j] = tmp;
    }
}

static void fb_scan_directory(gui_file_browser_state *state) {
    fb_free_entries(state);

    ARENA_SCRATCH_START();

    DirEntries raw = {0};
    da_init(&raw);

    const char *filter = (state->mode == GUI_FILE_BROWSER_FILE && state->ext_filter[0])
                             ? state->ext_filter
                             : nullptr;

    if (!platform_dir_scan(state->current_dir, filter, scratch.arena, &raw)) {
        RL_ERROR("gui_file_browser: failed to scan '%s'", state->current_dir);
        da_free(&raw);
        ARENA_SCRATCH_RELEASE();
        state->needs_scan = false;
        return;
    }

    // Copy names from scratch arena to mem_alloc
    for (u64 i = 0; i < raw.count; i++) {
        // In directory mode, skip files
        if (state->mode == GUI_FILE_BROWSER_DIRECTORY && !raw.items[i].is_dir) continue;

        u32 name_len = cstr_len(raw.items[i].name);
        char *copy = mem_alloc(name_len + 1, MEM_STRING);
        memcpy(copy, raw.items[i].name, name_len + 1);

        platform_dir_entry de = {.name = copy, .is_dir = raw.items[i].is_dir};
        da_append(&state->entries, de);
    }
    state->entries_owned = true;

    da_free(&raw);
    ARENA_SCRATCH_RELEASE();

    fb_sort_entries(&state->entries);

    // Sync path bar
    cstr_copy(state->path_input.buf, sizeof(state->path_input.buf), state->current_dir);
    state->path_input.len = (u16)cstr_len(state->path_input.buf);
    state->path_input.cursor = state->path_input.len;

    state->selected_index = -1;
    state->needs_scan = false;
}

static b8 fb_is_root(const char *path) {
#ifdef PLATFORM_WINDOWS
    u32 len = cstr_len(path);
    return len <= 3 && len >= 2 && path[1] == ':';
#else
    return path[0] == '/' && path[1] == '\0';
#endif
}

static void fb_navigate_to(gui_file_browser_state *state, const char *subdir) {
    u32 cur_len = cstr_len(state->current_dir);

    // Ensure trailing separator
    char sep = '/';
#ifdef PLATFORM_WINDOWS
    sep = '\\';
#endif
    if (cur_len > 0 && state->current_dir[cur_len - 1] != '/' &&
        state->current_dir[cur_len - 1] != '\\') {
        if (cur_len + 1 < sizeof(state->current_dir)) {
            state->current_dir[cur_len] = sep;
            state->current_dir[cur_len + 1] = '\0';
            cur_len++;
        }
    }

    // Append subdir
    u32 sub_len = cstr_len(subdir);
    if (cur_len + sub_len < sizeof(state->current_dir)) {
        memcpy(state->current_dir + cur_len, subdir, sub_len + 1);
    }

    state->needs_scan = true;
}

static void fb_navigate_up(gui_file_browser_state *state) {
    u32 len = cstr_len(state->current_dir);
    if (len == 0) return;

    // Strip trailing separator
    if (len > 1 && (state->current_dir[len - 1] == '/' || state->current_dir[len - 1] == '\\')) {
        state->current_dir[len - 1] = '\0';
        len--;
    }

#ifdef PLATFORM_WINDOWS
    // At drive root (e.g. "C:") → enter drives view
    if (len <= 2 && len >= 2 && state->current_dir[1] == ':') {
        state->current_dir[0] = '\0';
        state->needs_scan = true;
        return;
    }
#else
    // Already at root
    if (len == 1 && state->current_dir[0] == '/') return;
#endif

    // Find last separator
    i32 last_sep = -1;
    for (i32 i = (i32)len - 1; i >= 0; i--) {
        if (state->current_dir[i] == '/' || state->current_dir[i] == '\\') {
            last_sep = i;
            break;
        }
    }

    if (last_sep >= 0) {
#ifdef PLATFORM_WINDOWS
        // Keep drive root as "C:\"
        if (last_sep == 2 && state->current_dir[1] == ':') {
            state->current_dir[last_sep + 1] = '\0';
        } else {
            state->current_dir[last_sep] = '\0';
        }
#else
        // Keep "/" as root
        if (last_sep == 0) {
            state->current_dir[1] = '\0';
        } else {
            state->current_dir[last_sep] = '\0';
        }
#endif
    }

    state->needs_scan = true;
}

#ifdef PLATFORM_WINDOWS
static void fb_scan_drives(gui_file_browser_state *state) {
    fb_free_entries(state);

    ARENA_SCRATCH_START();

    DirEntries drives = {0};
    da_init(&drives);

    if (platform_list_drives(scratch.arena, &drives)) {
        for (u64 i = 0; i < drives.count; i++) {
            u32 name_len = cstr_len(drives.items[i].name);
            char *copy = mem_alloc(name_len + 1, MEM_STRING);
            memcpy(copy, drives.items[i].name, name_len + 1);

            platform_dir_entry de = {.name = copy, .is_dir = true};
            da_append(&state->entries, de);
        }
        state->entries_owned = true;
    }

    da_free(&drives);
    ARENA_SCRATCH_RELEASE();

    cstr_copy(state->path_input.buf, sizeof(state->path_input.buf), "");
    state->path_input.len = 0;
    state->path_input.cursor = 0;
    state->selected_index = -1;
    state->needs_scan = false;
}
#endif

// ---- Public API ----

void gui_file_browser_init(gui_file_browser_state *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->status = GUI_FILE_BROWSER_IDLE;
    state->selected_index = -1;
    da_init(&state->entries);
}

void gui_file_browser_shutdown(gui_file_browser_state *state) {
    if (!state) return;
    fb_free_entries(state);
}

void gui_file_browser_open(gui_file_browser_state *state,
                            gui_file_browser_mode mode,
                            const char *initial_path,
                            const char *ext_filter) {
    if (!state) return;

    state->mode = mode;
    state->status = GUI_FILE_BROWSER_OPEN;
    state->window.visible = true;
    state->selected_index = -1;
    state->last_click_index = -1;
    state->last_click_time = 0;
    state->time_acc = 0;

    if (ext_filter) {
        cstr_copy(state->ext_filter, sizeof(state->ext_filter), ext_filter);
    } else {
        state->ext_filter[0] = '\0';
    }

    if (initial_path && initial_path[0]) {
        cstr_copy(state->current_dir, sizeof(state->current_dir), initial_path);
    } else {
#ifdef PLATFORM_WINDOWS
        state->current_dir[0] = '\0'; // drives view
#else
        cstr_copy(state->current_dir, sizeof(state->current_dir), "/");
#endif
    }

    state->needs_scan = true;
}

gui_file_browser_status gui_file_browser_render(gui_file_browser_state *state,
                                                 f32 dt,
                                                 const gui_file_browser_cfg *cfg) {
    if (!state || state->status != GUI_FILE_BROWSER_OPEN) {
        return state ? state->status : GUI_FILE_BROWSER_IDLE;
    }

    const gui_theme *t = gui_theme_get();

    f32 width = (cfg && cfg->width > 0) ? cfg->width : 600;
    f32 height = (cfg && cfg->height > 0) ? cfg->height : 450;
    u16 font = (cfg && cfg->font) ? cfg->font : gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    u16 font_size = (cfg && cfg->font_size > 0) ? cfg->font_size : 13;

    gui_text_cfg label_text = {.color = t->text, .size = font_size, .font = font};
    gui_text_cfg dim_text = {.color = t->text_dim, .size = font_size, .font = font};

    gui_button_cfg btn_cfg = {
        .color = t->control,
        .hover_color = t->control_hover,
        .press_color = t->control_press,
        .padding = 8,
        .corner_radius = 4,
    };

    gui_text_input_render_cfg input_cfg = {
        .bg_color = t->bg_input,
        .text_color = t->text,
        .border_color = t->border,
        .border_width = 1,
        .padding = 6,
        .height = 26,
        .font = font,
        .font_size = font_size,
    };

    // -- Section 1: State sync --
    state->time_acc += dt;

    if (state->needs_scan) {
#ifdef PLATFORM_WINDOWS
        if (state->current_dir[0] == '\0') {
            fb_scan_drives(state);
        } else {
            fb_scan_directory(state);
        }
#else
        fb_scan_directory(state);
#endif
    }

    // -- Section 2: Layout --

    // Track interactions to apply in section 3
    b8 up_clicked = false;
    b8 ok_clicked = false;
    b8 cancel_clicked = false;
    b8 dotdot_clicked = false;
    i32 dir_clicked = -1;
    i32 file_clicked = -1;

    const char *title = (state->mode == GUI_FILE_BROWSER_DIRECTORY)
                            ? "Browse for Directory"
                            : "Browse for File";

    gui_window_cfg win_cfg = {
        .title = title,
        .width = width,
        .height = height,
        .bg_color = t->bg,
        .header_color = t->bg_secondary,
        .border_color = t->border,
        .corner_radius = 6,
        .font = font,
        .font_size = font_size,
        .z_index = 150,
    };

    gui_window_result wr = gui_window_begin(&state->window, &win_cfg);
    if (wr.close_clicked) {
        cancel_clicked = true;
    }

    if (wr.visible) {
        // Navigation row: [Up] [path input]
        GUI_ROW(6) {
            if (gui_text_button("Up", &btn_cfg, &label_text).clicked) {
                up_clicked = true;
            }
            gui_text_input_render(&state->path_input, dt, &input_cfg);
        }

        gui_separator();

        // Scrollable file list — children placed directly inside scroll
        gui_scroll_begin(&state->scroll, &(gui_scroll_cfg){
            .scrollbar_width = 8, .thumb_radius = 3,
        });

        // ".." entry if not at root and not in drives view
        b8 show_dotdot = true;
#ifdef PLATFORM_WINDOWS
        if (state->current_dir[0] == '\0') show_dotdot = false;
#endif
        if (show_dotdot && fb_is_root(state->current_dir)) show_dotdot = false;

        if (show_dotdot) {
            gui_button_cfg row_btn = {
                .color = {0, 0, 0, 0},
                .hover_color = t->control_hover,
                .press_color = t->control_press,
                .padding = 4,
                .corner_radius = 2,
                .grow_width = true,
            };
            if (gui_text_button("  ..  (parent directory)", &row_btn, &dim_text).clicked) {
                dotdot_clicked = true;
            }
        }

        // Directory/file entries
        rl_arena *frame = rl_engine_get_frame_arena();
        for (u64 i = 0; i < state->entries.count; i++) {
            const platform_dir_entry *e = &state->entries.items[i];

            b8 is_selected = ((i32)i == state->selected_index);

            if (e->is_dir) {
                // Directory row: clickable row navigates into it
                Clay_Color row_bg = is_selected ? t->accent : (Clay_Color){0, 0, 0, 0};
                gui_button_cfg row_btn = {
                    .color = row_bg,
                    .hover_color = t->control_hover,
                    .press_color = t->control_press,
                    .padding = 4,
                    .corner_radius = 2,
                    .grow_width = true,
                };
                const char *label = cstr_format(frame, "  > %s/", e->name);
                if (gui_text_button(label, &row_btn, &label_text).clicked) {
                    dir_clicked = (i32)i;
                }
            } else {
                // File row: click selects, double-click confirms
                Clay_Color row_bg = is_selected ? t->accent : (Clay_Color){0, 0, 0, 0};
                Clay_Color row_hover = is_selected ? t->accent_hover : t->control_hover;
                gui_button_cfg row_btn = {
                    .color = row_bg,
                    .hover_color = row_hover,
                    .press_color = t->control_press,
                    .padding = 4,
                    .corner_radius = 2,
                    .grow_width = true,
                };
                const char *label = cstr_format(frame, "    %s", e->name);
                if (gui_text_button(label, &row_btn, &dim_text).clicked) {
                    file_clicked = (i32)i;
                }
            }
        }

        gui_scroll_end();

        gui_separator();

        // Action row
        GUI_ROW(8) {
            gui_spacer();
            if (gui_text_button("Cancel", &btn_cfg, &label_text).clicked) {
                cancel_clicked = true;
            }

            const char *ok_label = (state->mode == GUI_FILE_BROWSER_DIRECTORY)
                                       ? "Select Folder"
                                       : "Open";
            if (gui_text_button(ok_label, &btn_cfg, &label_text).clicked) {
                ok_clicked = true;
            }
        }
    }
    gui_window_end();

    // -- Section 3: Apply changes --

    if (cancel_clicked) {
        state->status = GUI_FILE_BROWSER_CANCELLED;
        state->window.visible = false;
        state->result_path[0] = '\0';
        return state->status;
    }

    if (up_clicked || dotdot_clicked) {
        fb_navigate_up(state);
    }

    // Directory clicked → navigate into it immediately
    if (dir_clicked >= 0) {
        const platform_dir_entry *e = &state->entries.items[dir_clicked];
#ifdef PLATFORM_WINDOWS
        if (state->current_dir[0] == '\0') {
            cstr_copy(state->current_dir, sizeof(state->current_dir), e->name);
            state->needs_scan = true;
        } else {
            fb_navigate_to(state, e->name);
        }
#else
        fb_navigate_to(state, e->name);
#endif
    }

    // File clicked → select; double-click confirms
    if (file_clicked >= 0) {
        state->selected_index = file_clicked;

        f32 click_time = state->time_acc;
        b8 is_double = (file_clicked == state->last_click_index &&
                        (click_time - state->last_click_time) < 0.4f);
        state->last_click_time = click_time;
        state->last_click_index = file_clicked;

        if (is_double) {
            const char *sel = state->entries.items[file_clicked].name;
            cstr_format_buf(state->result_path, sizeof(state->result_path),
                            "%s/%s", state->current_dir, sel);
            state->status = GUI_FILE_BROWSER_CONFIRMED;
            state->window.visible = false;
            return state->status;
        }
    }

    if (ok_clicked) {
        if (state->mode == GUI_FILE_BROWSER_DIRECTORY) {
            // Confirm current directory
            cstr_copy(state->result_path, sizeof(state->result_path), state->current_dir);
            state->status = GUI_FILE_BROWSER_CONFIRMED;
            state->window.visible = false;
            return state->status;
        } else {
            // File mode: need a file selected
            if (state->selected_index >= 0 &&
                (u64)state->selected_index < state->entries.count &&
                !state->entries.items[state->selected_index].is_dir) {
                const char *sel = state->entries.items[state->selected_index].name;
                cstr_format_buf(state->result_path, sizeof(state->result_path),
                                "%s/%s", state->current_dir, sel);
                state->status = GUI_FILE_BROWSER_CONFIRMED;
                state->window.visible = false;
                return state->status;
            }
        }
    }

    return state->status;
}

b8 gui_file_browser_handle_key(gui_file_browser_state *state, void *key_data) {
    if (!state || state->status != GUI_FILE_BROWSER_OPEN) return false;

    input_key *key = key_data;
    if (!key->pressed) return false;

    // Escape → cancel
    if (key->key == KEY_ESCAPE) {
        state->status = GUI_FILE_BROWSER_CANCELLED;
        state->window.visible = false;
        state->result_path[0] = '\0';
        return true;
    }

    // Enter → navigate to typed path, or confirm selection
    if (key->key == KEY_ENTER) {
        // If path bar has changed, navigate there
        if (state->path_input.buf[0] && strcmp(state->path_input.buf, state->current_dir) != 0) {
            cstr_copy(state->current_dir, sizeof(state->current_dir), state->path_input.buf);
            state->needs_scan = true;
            return true;
        }
        // Selected entry: dir → enter, file → confirm
        if (state->selected_index >= 0 &&
            (u64)state->selected_index < state->entries.count) {
            const platform_dir_entry *e = &state->entries.items[state->selected_index];
            if (e->is_dir) {
#ifdef PLATFORM_WINDOWS
                if (state->current_dir[0] == '\0') {
                    cstr_copy(state->current_dir, sizeof(state->current_dir), e->name);
                    state->needs_scan = true;
                } else {
                    fb_navigate_to(state, e->name);
                }
#else
                fb_navigate_to(state, e->name);
#endif
                return true;
            } else {
                // Confirm selected file
                cstr_format_buf(state->result_path, sizeof(state->result_path),
                                "%s/%s", state->current_dir, e->name);
                state->status = GUI_FILE_BROWSER_CONFIRMED;
                state->window.visible = false;
                return true;
            }
        }
        return true;
    }

    // Arrow keys to move selection
    if (key->key == KEY_UP) {
        if (state->selected_index > 0) {
            state->selected_index--;
        }
        return true;
    }
    if (key->key == KEY_DOWN) {
        if (state->selected_index < (i32)state->entries.count - 1) {
            state->selected_index++;
        }
        return true;
    }

    // Backspace → navigate up (when path bar doesn't consume it)
    if (key->key == KEY_BACKSPACE && state->path_input.len == 0) {
        fb_navigate_up(state);
        return true;
    }

    // Forward to path bar
    return gui_text_input_handle_key(&state->path_input, key);
}

b8 gui_file_browser_handle_char(gui_file_browser_state *state, void *char_data) {
    if (!state || state->status != GUI_FILE_BROWSER_OPEN) return false;

    gui_text_input_handle_char(&state->path_input, char_data);
    return true;
}

#include "gui/gui_file_browser.h"

#include "asset/asset.h"
#include "core/logger.h"
#include "engine.h"
#include "gui/gui_button.h"
#include "gui/gui_clay.h"
#include "gui/gui_icon.h"
#include "gui/gui_panel.h"
#include "gui/gui_text.h"
#include "gui/gui_text_input.h"
#include "gui/gui_theme.h"
#include "gui/gui_scroll.h"
#include "gui/gui_tree.h"
#include "gui/gui_window.h"
#include "memory/arena.h"
#include "memory/memory.h"
#include "platform/input.h"
#include "platform/io/file_scan.h"
#include "util/hash.h"
#include "util/str.h"

#include <string.h>

#ifdef PLATFORM_WINDOWS
#define FB_SEP '\\'
#else
#define FB_SEP '/'
#endif

// ---- Node helpers ----

static u32 fb_path_id(const char *path) {
    return (u32)hash_fnv1a(path, cstr_len(path));
}

static void fb_node_free_children(fb_tree_node *node) {
    if (node->children_owned) {
        for (u64 i = 0; i < node->children.count; i++) {
            if (node->children.items[i].name) {
                u32 len = cstr_len(node->children.items[i].name);
                mem_free((void *)node->children.items[i].name, len + 1, MEM_STRING);
            }
        }
    }
    da_free(&node->children);
    da_init(&node->children);
    node->children_owned = false;
}

// ---- Pool functions ----

static fb_tree_node *fb_pool_find(fb_tree_pool *pool, const char *path) {
    for (u64 i = 0; i < pool->nodes.count; i++) {
        if (cstr_eq(pool->nodes.items[i].path, path)) {
            return &pool->nodes.items[i];
        }
    }
    return nullptr;
}

static fb_tree_node *fb_pool_add(fb_tree_pool *pool, const char *path) {
    fb_tree_node node = {0};
    cstr_copy(node.path, sizeof(node.path), path);
    da_init(&node.children);
    da_append(&pool->nodes, node);
    return &pool->nodes.items[pool->nodes.count - 1];
}

static fb_tree_node *fb_pool_find_or_add(fb_tree_pool *pool, const char *path) {
    fb_tree_node *n = fb_pool_find(pool, path);
    if (n) return n;
    return fb_pool_add(pool, path);
}

static void fb_pool_remove_subtree(fb_tree_pool *pool, const char *prefix) {
    u32 prefix_len = cstr_len(prefix);
    b8 prefix_has_sep = (prefix_len > 0 &&
                          (prefix[prefix_len - 1] == '/' || prefix[prefix_len - 1] == '\\'));

    for (u64 i = 0; i < pool->nodes.count; ) {
        fb_tree_node *n = &pool->nodes.items[i];
        u32 n_len = cstr_len(n->path);

        b8 is_descendant = false;
        if (n_len > prefix_len && memcmp(n->path, prefix, prefix_len) == 0) {
            if (prefix_has_sep) {
                is_descendant = true;
            } else {
                is_descendant = (n->path[prefix_len] == '/' || n->path[prefix_len] == '\\');
            }
        }

        if (is_descendant) {
            fb_node_free_children(n);
            pool->nodes.items[i] = pool->nodes.items[pool->nodes.count - 1];
            pool->nodes.count--;
        } else {
            i++;
        }
    }
}

static void fb_pool_clear(fb_tree_pool *pool) {
    for (u64 i = 0; i < pool->nodes.count; i++) {
        fb_node_free_children(&pool->nodes.items[i]);
    }
    da_free(&pool->nodes);
    da_init(&pool->nodes);
}

// ---- Sorting ----

static i32 fb_compare_entries(const platform_dir_entry *a, const platform_dir_entry *b) {
    if (a->is_dir && !b->is_dir) return -1;
    if (!a->is_dir && b->is_dir) return 1;
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

// ---- Scanning ----

static void fb_scan_into_node(fb_tree_node *node, const char *ext_filter,
                               gui_file_browser_mode mode) {
    fb_node_free_children(node);

    ARENA_SCRATCH_START();

    DirEntries raw = {0};
    da_init(&raw);

    const char *filter = (mode == GUI_FILE_BROWSER_FILE && ext_filter[0])
                             ? ext_filter
                             : nullptr;

    if (!platform_dir_scan(node->path, filter, scratch.arena, &raw)) {
        RL_ERROR("gui_file_browser: failed to scan '%s'", node->path);
        da_free(&raw);
        ARENA_SCRATCH_RELEASE();
        node->scanned = true;
        return;
    }

    for (u64 i = 0; i < raw.count; i++) {
        if (mode == GUI_FILE_BROWSER_DIRECTORY && !raw.items[i].is_dir) continue;

        u32 name_len = cstr_len(raw.items[i].name);
        char *copy = mem_alloc(name_len + 1, MEM_STRING);
        memcpy(copy, raw.items[i].name, name_len + 1);

        platform_dir_entry de = {.name = copy, .is_dir = raw.items[i].is_dir};
        da_append(&node->children, de);
    }
    node->children_owned = true;

    da_free(&raw);
    ARENA_SCRATCH_RELEASE();

    fb_sort_entries(&node->children);
    node->scanned = true;
}

// ---- Path helpers ----

static void fb_build_child_path(char *dst, u32 dst_size,
                                 const char *parent, const char *child_name) {
    u32 parent_len = cstr_len(parent);
    b8 has_sep = (parent_len > 0 &&
                  (parent[parent_len - 1] == '/' || parent[parent_len - 1] == '\\'));
    if (has_sep) {
        cstr_format_buf(dst, dst_size, "%s%s", parent, child_name);
    } else {
        cstr_format_buf(dst, dst_size, "%s%c%s", parent, FB_SEP, child_name);
    }
}

static const char *fb_display_name(const char *path, rl_arena *frame) {
    u32 len = cstr_len(path);
    if (len == 0) return "";

#ifdef PLATFORM_WINDOWS
    if (len <= 3 && path[1] == ':') {
        return cstr_format(frame, "%s", path);
    }
#else
    if (len == 1 && path[0] == '/') return "/";
#endif

    i32 end = (i32)len - 1;
    if (path[end] == '/' || path[end] == '\\') end--;

    i32 start = end;
    while (start > 0 && path[start - 1] != '/' && path[start - 1] != '\\') start--;

    u32 name_len = (u32)(end - start + 1);
    char *name = rl_arena_push(frame, name_len + 1, false);
    memcpy(name, path + start, name_len);
    name[name_len] = '\0';
    return name;
}

// ---- Recursive tree renderer ----

static void fb_render_dir_node(gui_file_browser_state *state, fb_tree_node *node,
                                const gui_text_cfg *label_text, const gui_text_cfg *dim_text,
                                rl_arena *frame, i32 depth, b8 *confirmed) {
    if (depth > 30) return;

    u32 node_id = fb_path_id(node->path);
    const char *display = fb_display_name(node->path, frame);

    b8 was_expanded = node->expanded;
    gui_icon_type dir_icon = node->expanded ? GUI_ICON_FOLDER_OPEN : GUI_ICON_FOLDER;
    gui_tree_node_result result = gui_tree_node_begin(node_id, display, &node->expanded, false, dir_icon);

    if (result.selected) {
        cstr_copy(state->selected_path, sizeof(state->selected_path), node->path);
        state->selected_is_dir = true;
        cstr_copy(state->path_input.buf, sizeof(state->path_input.buf), node->path);
        state->path_input.len = (u16)cstr_len(state->path_input.buf);
        state->path_input.cursor = state->path_input.len;
    }

    if (node->expanded) {
        if (!node->scanned) {
            fb_scan_into_node(node, state->ext_filter, state->mode);
        }

        for (u64 i = 0; i < node->children.count; i++) {
            const platform_dir_entry *e = &node->children.items[i];

            char child_path[512];
            fb_build_child_path(child_path, sizeof(child_path), node->path, e->name);

            if (e->is_dir) {
                fb_tree_node *child_node = fb_pool_find_or_add(&state->pool, child_path);
                fb_render_dir_node(state, child_node, label_text, dim_text,
                                   frame, depth + 1, confirmed);
            } else {
                u32 file_id = fb_path_id(child_path);
                gui_tree_node_result fr = gui_tree_node_begin(file_id, e->name, nullptr, true, GUI_ICON_FILE);
                if (fr.selected) {
                    cstr_copy(state->selected_path, sizeof(state->selected_path), child_path);
                    state->selected_is_dir = false;
                    cstr_copy(state->path_input.buf, sizeof(state->path_input.buf), child_path);
                    state->path_input.len = (u16)cstr_len(state->path_input.buf);
                    state->path_input.cursor = state->path_input.len;

                    // Double-click detection
                    f32 click_time = state->time_acc;
                    b8 is_double = (file_id == state->last_click_node_id &&
                                    (click_time - state->last_click_time) < 0.4f);
                    state->last_click_time = click_time;
                    state->last_click_node_id = file_id;

                    if (is_double) {
                        *confirmed = true;
                    }
                }
                gui_tree_node_end();
            }
        }
    }

    // Collapsed after being expanded — evict descendants
    if (was_expanded && !node->expanded) {
        fb_pool_remove_subtree(&state->pool, node->path);
        fb_node_free_children(node);
        node->scanned = false;
    }

    gui_tree_node_end();
}

// ---- Path navigation ----

static void fb_expand_to_path(gui_file_browser_state *state, const char *target) {
    char current[512] = {0};

#ifdef PLATFORM_WINDOWS
    if (cstr_len(target) >= 2 && target[1] == ':') {
        current[0] = target[0];
        current[1] = ':';
        current[2] = '\\';
        current[3] = '\0';

        if (state->root_path[0] == '\0') {
            cstr_copy(state->root_path, sizeof(state->root_path), current);
        }
    }
    const char *rest = target + 3;
    if (cstr_len(target) >= 3 && (target[2] == '\\' || target[2] == '/')) {
        rest = target + 3;
    } else if (cstr_len(target) >= 2) {
        rest = target + 2;
    }
#else
    current[0] = '/';
    current[1] = '\0';
    const char *rest = target + 1;
#endif

    fb_tree_node *node = fb_pool_find_or_add(&state->pool, current);
    node->expanded = true;
    if (!node->scanned) {
        fb_scan_into_node(node, state->ext_filter, state->mode);
    }

    while (*rest) {
        const char *sep = rest;
        while (*sep && *sep != '/' && *sep != '\\') sep++;

        u32 comp_len = (u32)(sep - rest);
        if (comp_len == 0) { rest = *sep ? sep + 1 : sep; continue; }

        char component[256];
        if (comp_len >= sizeof(component)) comp_len = sizeof(component) - 1;
        memcpy(component, rest, comp_len);
        component[comp_len] = '\0';

        char next_path[512];
        fb_build_child_path(next_path, sizeof(next_path), current, component);

        fb_tree_node *next_node = fb_pool_find_or_add(&state->pool, next_path);
        next_node->expanded = true;
        if (!next_node->scanned) {
            fb_scan_into_node(next_node, state->ext_filter, state->mode);
        }

        cstr_copy(current, sizeof(current), next_path);
        node = next_node;

        rest = *sep ? sep + 1 : sep;
    }

    cstr_copy(state->selected_path, sizeof(state->selected_path), target);
    state->selected_is_dir = true;
    state->tree.selected_id = fb_path_id(target);
}

static void fb_navigate_up(gui_file_browser_state *state) {
#ifdef PLATFORM_WINDOWS
    if (state->root_path[0] == '\0') return;
#endif

    u32 len = cstr_len(state->root_path);
    if (len == 0) return;

    char parent[512];
    cstr_copy(parent, sizeof(parent), state->root_path);

    // Strip trailing separator
    if (len > 1 && (parent[len - 1] == '/' || parent[len - 1] == '\\')) {
        parent[len - 1] = '\0';
        len--;
    }

#ifdef PLATFORM_WINDOWS
    if (len <= 2 && parent[1] == ':') {
        fb_pool_clear(&state->pool);
        state->root_path[0] = '\0';
        state->selected_path[0] = '\0';
        cstr_copy(state->path_input.buf, sizeof(state->path_input.buf), "");
        state->path_input.len = 0;
        state->path_input.cursor = 0;
        return;
    }
#else
    if (len == 1 && parent[0] == '/') return;
#endif

    i32 last_sep = -1;
    for (i32 i = (i32)len - 1; i >= 0; i--) {
        if (parent[i] == '/' || parent[i] == '\\') {
            last_sep = i;
            break;
        }
    }

    if (last_sep >= 0) {
#ifdef PLATFORM_WINDOWS
        if (last_sep == 2 && parent[1] == ':') {
            parent[last_sep + 1] = '\0';
        } else {
            parent[last_sep] = '\0';
        }
#else
        if (last_sep == 0) {
            parent[1] = '\0';
        } else {
            parent[last_sep] = '\0';
        }
#endif
    }

    fb_pool_clear(&state->pool);
    cstr_copy(state->root_path, sizeof(state->root_path), parent);

    fb_tree_node *root = fb_pool_find_or_add(&state->pool, state->root_path);
    root->expanded = true;

    cstr_copy(state->path_input.buf, sizeof(state->path_input.buf), state->root_path);
    state->path_input.len = (u16)cstr_len(state->path_input.buf);
    state->path_input.cursor = state->path_input.len;
    state->selected_path[0] = '\0';
}

// ---- Public API ----

void gui_file_browser_init(gui_file_browser_state *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->status = GUI_FILE_BROWSER_IDLE;
    da_init(&state->pool.nodes);
}

void gui_file_browser_shutdown(gui_file_browser_state *state) {
    if (!state) return;
    fb_pool_clear(&state->pool);
}

void gui_file_browser_open(gui_file_browser_state *state,
                            gui_file_browser_mode mode,
                            const char *initial_path,
                            const char *ext_filter) {
    if (!state) return;

    state->mode = mode;
    state->status = GUI_FILE_BROWSER_OPEN;
    state->window.visible = true;
    state->selected_path[0] = '\0';
    state->selected_is_dir = false;
    state->last_click_node_id = 0;
    state->last_click_time = 0;
    state->time_acc = 0;

    if (ext_filter) {
        cstr_copy(state->ext_filter, sizeof(state->ext_filter), ext_filter);
    } else {
        state->ext_filter[0] = '\0';
    }

    fb_pool_clear(&state->pool);

    if (initial_path && initial_path[0]) {
#ifdef PLATFORM_WINDOWS
        if (cstr_len(initial_path) >= 2 && initial_path[1] == ':') {
            char drive[4] = {initial_path[0], ':', '\\', '\0'};
            cstr_copy(state->root_path, sizeof(state->root_path), drive);
        } else {
            state->root_path[0] = '\0';
        }
#else
        cstr_copy(state->root_path, sizeof(state->root_path), "/");
#endif
        fb_expand_to_path(state, initial_path);

        cstr_copy(state->path_input.buf, sizeof(state->path_input.buf), initial_path);
        state->path_input.len = (u16)cstr_len(state->path_input.buf);
        state->path_input.cursor = state->path_input.len;
    } else {
#ifdef PLATFORM_WINDOWS
        state->root_path[0] = '\0';
        state->path_input.buf[0] = '\0';
        state->path_input.len = 0;
        state->path_input.cursor = 0;
#else
        cstr_copy(state->root_path, sizeof(state->root_path), "/");
        fb_tree_node *root = fb_pool_find_or_add(&state->pool, "/");
        root->expanded = true;
        cstr_copy(state->path_input.buf, sizeof(state->path_input.buf), "/");
        state->path_input.len = 1;
        state->path_input.cursor = 1;
#endif
    }
}

gui_file_browser_status gui_file_browser_render(gui_file_browser_state *state,
                                                 f32 dt,
                                                 const gui_file_browser_cfg *cfg) {
    if (!state || state->status != GUI_FILE_BROWSER_OPEN) {
        return state ? state->status : GUI_FILE_BROWSER_IDLE;
    }

    const gui_theme *t = gui_theme_get();

    b8 docked = cfg && cfg->docked;
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

    // -- Section 2: Layout --
    b8 up_clicked = false;
    b8 ok_clicked = false;
    b8 cancel_clicked = false;
    b8 confirmed = false;

    const char *title = (state->mode == GUI_FILE_BROWSER_DIRECTORY)
                            ? "Browse for Directory"
                            : "Browse for File";

    b8 visible = false;

    if (docked) {
        // Docked: render as a panel filling available space
        gui_panel_cfg dock_panel = {
            .color = t->bg_secondary,
            .width_sizing = GUI_SIZE_GROW,
            .height_sizing = GUI_SIZE_GROW,
            .corner_radius = 8,
            .padding = 0,
        };
        gui_panel_begin(&dock_panel);
        visible = true;

        // Title header
        gui_panel_cfg header = {
            .color = t->bg_secondary,
            .width_sizing = GUI_SIZE_GROW,
            .padding = 12,
        };
        GUI_PANEL(&header) {
            gui_text_cfg title_cfg = {.color = t->text, .size = font_size + 2, .font = font};
            gui_text(title, &title_cfg);
        }
        gui_separator();
    } else {
        // Floating window mode
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
        visible = wr.visible;
    }

    if (visible) {
        // Navigation row: [Up] [path input]
        gui_panel_cfg nav_row = {
            .horizontal = true, .gap = 6,
            .width_sizing = GUI_SIZE_GROW,
            .padding = 8,
        };
        GUI_PANEL(&nav_row) {
            {
                gui_button_cfg up_btn = btn_cfg;
                up_btn.gap = 6;
                gui_button_state up_state = gui_button_begin(&up_btn);
                gui_icon(GUI_ICON_ARROW_UP, font_size, label_text.color);
                gui_text("Up", &label_text);
                gui_button_end();
                if (up_state.clicked) up_clicked = true;
            }
            gui_text_input_render(&state->path_input, dt, &input_cfg);
        }

        gui_separator();

        // Scrollable tree
        gui_scroll_begin(&state->scroll, &(gui_scroll_cfg){
            .scrollbar_width = 8, .thumb_radius = 3,
        });

        rl_arena *frame = rl_engine_get_frame_arena();

        // Opaque selection color — prevents bleed-through
        gui_tree_cfg tcfg = {
            .font = font,
            .font_size = font_size,
            .select_color = {t->accent.r, t->accent.g, t->accent.b, 255},
        };
        gui_tree_begin(&state->tree, &tcfg);

#ifdef PLATFORM_WINDOWS
        if (state->root_path[0] == '\0') {
            // Drives view — populate pool on first frame
            b8 has_drives = false;
            for (u64 i = 0; i < state->pool.nodes.count; i++) {
                u32 plen = cstr_len(state->pool.nodes.items[i].path);
                if (plen <= 3 && plen >= 2 && state->pool.nodes.items[i].path[1] == ':') {
                    has_drives = true;
                    break;
                }
            }
            if (!has_drives) {
                ARENA_SCRATCH_START();
                DirEntries drives = {0};
                da_init(&drives);
                if (platform_list_drives(scratch.arena, &drives)) {
                    for (u64 i = 0; i < drives.count; i++) {
                        fb_pool_find_or_add(&state->pool, drives.items[i].name);
                    }
                }
                da_free(&drives);
                ARENA_SCRATCH_RELEASE();
            }
            // Render drive nodes
            for (u64 i = 0; i < state->pool.nodes.count; i++) {
                fb_tree_node *n = &state->pool.nodes.items[i];
                u32 plen = cstr_len(n->path);
                if (plen <= 3 && plen >= 2 && n->path[1] == ':') {
                    fb_render_dir_node(state, n, &label_text, &dim_text,
                                       frame, 0, &confirmed);
                }
            }
        } else {
            fb_tree_node *root = fb_pool_find_or_add(&state->pool, state->root_path);
            fb_render_dir_node(state, root, &label_text, &dim_text,
                               frame, 0, &confirmed);
        }
#else
        {
            fb_tree_node *root = fb_pool_find_or_add(&state->pool, state->root_path);
            fb_render_dir_node(state, root, &label_text, &dim_text,
                               frame, 0, &confirmed);
        }
#endif

        gui_tree_end();

        gui_scroll_end();

        gui_separator();

        // Action row
        gui_panel_cfg action_row = {
            .horizontal = true, .gap = 8,
            .width_sizing = GUI_SIZE_GROW,
            .padding = 8,
        };
        GUI_PANEL(&action_row) {
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

    if (docked) {
        gui_panel_end();
    } else {
        gui_window_end();
    }

    // -- Section 3: Apply changes --

    if (cancel_clicked) {
        state->status = GUI_FILE_BROWSER_CANCELLED;
        state->window.visible = false;
        state->result_path[0] = '\0';
        return state->status;
    }

    if (up_clicked) {
        fb_navigate_up(state);
    }

    // Double-click on file confirms immediately
    if (confirmed && state->selected_path[0] && !state->selected_is_dir) {
        cstr_copy(state->result_path, sizeof(state->result_path), state->selected_path);
        state->status = GUI_FILE_BROWSER_CONFIRMED;
        state->window.visible = false;
        return state->status;
    }

    if (ok_clicked) {
        if (state->mode == GUI_FILE_BROWSER_DIRECTORY) {
            const char *path = state->selected_path[0] ? state->selected_path : state->root_path;
            if (path[0]) {
                cstr_copy(state->result_path, sizeof(state->result_path), path);
                state->status = GUI_FILE_BROWSER_CONFIRMED;
                state->window.visible = false;
                return state->status;
            }
        } else {
            if (state->selected_path[0] && !state->selected_is_dir) {
                cstr_copy(state->result_path, sizeof(state->result_path), state->selected_path);
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

    // Escape -> cancel
    if (key->key == KEY_ESCAPE) {
        state->status = GUI_FILE_BROWSER_CANCELLED;
        state->window.visible = false;
        state->result_path[0] = '\0';
        return true;
    }

    // Enter -> navigate to typed path, or confirm selection
    if (key->key == KEY_ENTER) {
        const char *current_ref = state->selected_path[0] ? state->selected_path : state->root_path;
        if (state->path_input.buf[0] && !cstr_eq(state->path_input.buf, current_ref)) {
            // Path bar changed — navigate there
            fb_pool_clear(&state->pool);
#ifdef PLATFORM_WINDOWS
            if (cstr_len(state->path_input.buf) >= 2 && state->path_input.buf[1] == ':') {
                char drive[4] = {state->path_input.buf[0], ':', '\\', '\0'};
                cstr_copy(state->root_path, sizeof(state->root_path), drive);
            }
#else
            cstr_copy(state->root_path, sizeof(state->root_path), "/");
#endif
            fb_expand_to_path(state, state->path_input.buf);
            return true;
        }

        // Confirm selection
        if (state->selected_path[0]) {
            if (state->mode == GUI_FILE_BROWSER_DIRECTORY && state->selected_is_dir) {
                cstr_copy(state->result_path, sizeof(state->result_path), state->selected_path);
                state->status = GUI_FILE_BROWSER_CONFIRMED;
                state->window.visible = false;
                return true;
            }
            if (state->mode == GUI_FILE_BROWSER_FILE && !state->selected_is_dir) {
                cstr_copy(state->result_path, sizeof(state->result_path), state->selected_path);
                state->status = GUI_FILE_BROWSER_CONFIRMED;
                state->window.visible = false;
                return true;
            }
        }
        return true;
    }

    // Backspace when path bar is empty -> navigate root up
    if (key->key == KEY_BACKSPACE && state->path_input.len == 0) {
        fb_navigate_up(state);
        return true;
    }

    return gui_text_input_handle_key(&state->path_input, key);
}

b8 gui_file_browser_handle_char(gui_file_browser_state *state, void *char_data) {
    if (!state || state->status != GUI_FILE_BROWSER_OPEN) return false;

    gui_text_input_handle_char(&state->path_input, char_data);
    return true;
}

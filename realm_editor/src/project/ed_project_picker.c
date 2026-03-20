#include "project/ed_project_picker.h"

#include "core/ed_application.h"
#include "core/ed_config.h"
#include "asset/asset.h"
#include "core/event.h"
#include "core/logger.h"
#include "gui/gui_focus.h"
#include "core/project.h"
#include "gui/gui_button.h"
#include "gui/gui_clay.h"
#include "gui/gui_icon.h"
#include "gui/gui_panel.h"
#include "gui/gui_text.h"
#include "gui/gui_text_input.h"
#include "gui/gui_theme.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#include <string.h>

static b8 on_key_press(void *data, void *user_data) {
    ed_project_picker *picker = user_data;
    if (!picker || !picker->active) return false;

    input_key *key = data;
    if (!key->pressed) return false;

    // File browser intercepts input when open
    if (picker->file_browser.status == GUI_FILE_BROWSER_OPEN) {
        return gui_file_browser_handle_key(&picker->file_browser, key);
    }

    // Tab cycles focus between inputs in new project view
    if (key->key == KEY_TAB && picker->view == ED_PICKER_NEW_PROJECT) {
        gui_text_input_state *next = gui_focus_is(picker->path_input._id)
            ? &picker->name_input : &picker->path_input;
        gui_focus_set(next->_id);
        next->cursor_blink = 0;
        return true;
    }

    // Escape returns to home view
    if (key->key == KEY_ESCAPE) {
        if (picker->view != ED_PICKER_HOME) {
            picker->view = ED_PICKER_HOME;
            picker->error_msg[0] = '\0';
            return true;
        }
        return false;
    }

    return false;
}

static b8 on_char_input(void *data, void *user_data) {
    ed_project_picker *picker = user_data;
    if (!picker || !picker->active) return false;

    if (picker->file_browser.status == GUI_FILE_BROWSER_OPEN) {
        return gui_file_browser_handle_char(&picker->file_browser, data);
    }

    return false;
}

void ed_project_picker_init(ed_project_picker *picker) {
    if (!picker) return;
    memset(picker, 0, sizeof(*picker));
    picker->view = ED_PICKER_HOME;
    picker->active = true;
    gui_file_browser_init(&picker->file_browser);

    event_register(EVENT_KEY_PRESS, on_key_press, picker);
    event_register(EVENT_CHAR_INPUT, on_char_input, picker);
}

static void picker_try_open(ed_project_picker *picker, ed_application *app, const char *path) {
    if (!path || !path[0]) {
        cstr_copy(picker->error_msg, sizeof(picker->error_msg), "Path cannot be empty");
        return;
    }

    rl_project *proj = project_open(path);
    if (!proj) {
        cstr_copy(picker->error_msg, sizeof(picker->error_msg), "Failed to open project (missing project.realm?)");
        return;
    }

    picker->project_selected = true;
    picker->error_msg[0] = '\0';
}

static void picker_try_create(ed_project_picker *picker, ed_application *app) {
    const char *path = picker->path_input.buf;
    const char *name = picker->name_input.buf;

    if (!path[0]) {
        cstr_copy(picker->error_msg, sizeof(picker->error_msg), "Project path cannot be empty");
        return;
    }
    if (!name[0]) {
        cstr_copy(picker->error_msg, sizeof(picker->error_msg), "Project name cannot be empty");
        return;
    }

    if (!project_create(path, name)) {
        cstr_copy(picker->error_msg, sizeof(picker->error_msg), "Failed to create project");
        return;
    }

    rl_project *proj = project_open(path);
    if (!proj) {
        cstr_copy(picker->error_msg, sizeof(picker->error_msg), "Project created but failed to open");
        return;
    }

    picker->project_selected = true;
    picker->error_msg[0] = '\0';
}

void ed_project_picker_render(ed_project_picker *picker, ed_application *app, f32 dt) {
    if (!picker || !app) return;

    // Handle Enter-triggered submit from centralized router
    if (picker->path_input.submitted || picker->name_input.submitted) {
        picker->path_input.submitted = false;
        picker->name_input.submitted = false;
        if (picker->view == ED_PICKER_NEW_PROJECT) {
            picker_try_create(picker, app);
        } else if (picker->view == ED_PICKER_OPEN_PROJECT) {
            picker_try_open(picker, app, picker->path_input.buf);
        }
    }

    const gui_theme *t = gui_theme_get();
    u16 font = gui_font_id(asset_find(RL_ASSET_FONT_JETBRAINS_MONO));
    gui_text_cfg title_text = {.color = t->text, .size = 22, .font = font};
    gui_text_cfg label_text = {.color = t->text, .size = 13, .font = font};
    gui_text_cfg dim_text   = {.color = t->text_dim, .size = 13, .font = font};
    gui_text_cfg error_text = {.color = t->danger, .size = 13, .font = font};

    gui_button_cfg btn_cfg = {
        .color = t->control,
        .hover_color = t->control_hover,
        .press_color = t->control_press,
        .padding = 10,
        .corner_radius = 4,
    };

    gui_text_input_render_cfg input_cfg = {
        .bg_color = t->bg_input,
        .text_color = t->text,
        .border_color = t->border,
        .border_width = 1,
        .padding = 8,
        .height = 28,
        .font = font,
        .font_size = 13,
    };

    gui_text_input_render_cfg input_cfg_dim = input_cfg;
    input_cfg_dim.border_color = t->control_press;

    b8 fb_open = (picker->file_browser.status == GUI_FILE_BROWSER_OPEN);

    // Full-screen centered root
    gui_panel_cfg root = {
        .color = t->bg,
        .width_sizing = GUI_SIZE_GROW,
        .height_sizing = GUI_SIZE_GROW,
        .align_x = CLAY_ALIGN_X_CENTER,
        .align_y = CLAY_ALIGN_Y_CENTER,
        .padding = fb_open ? 24 : 0,
    };
    GUI_PANEL(&root) {
        if (fb_open) {
            // File browser replaces the card in docked mode
            gui_file_browser_cfg fb_cfg = {.docked = true};
            gui_file_browser_status fb_status = gui_file_browser_render(
                &picker->file_browser, dt, &fb_cfg);
            if (fb_status == GUI_FILE_BROWSER_CONFIRMED) {
                cstr_copy(picker->path_input.buf, sizeof(picker->path_input.buf),
                          picker->file_browser.result_path);
                picker->path_input.len = (u16)cstr_len(picker->path_input.buf);
                picker->path_input.cursor = picker->path_input.len;
            }
        } else {
            // Card
            gui_panel_cfg card = {
                .color = t->bg_secondary,
                .width = 500,
                .padding = 24,
                .gap = 12,
                .corner_radius = 8,
            };
            GUI_PANEL(&card) {
                switch (picker->view) {
                case ED_PICKER_HOME: {
                    gui_text("Realm Editor", &title_text);
                    gui_separator();

                    // Buttons row
                    GUI_ROW(8) {
                        {
                            gui_button_cfg nb = btn_cfg;
                            nb.gap = 6;
                            gui_button_state ns = gui_button_begin(&nb);
                            gui_icon(GUI_ICON_PLUS, 13, label_text.color);
                            gui_text("New Project", &label_text);
                            gui_button_end();
                            if (ns.clicked) {
                                picker->view = ED_PICKER_NEW_PROJECT;
                                picker->error_msg[0] = '\0';
                                memset(&picker->path_input, 0, sizeof(picker->path_input));
                                memset(&picker->name_input, 0, sizeof(picker->name_input));
                            }
                        }
                        {
                            gui_button_cfg ob = btn_cfg;
                            ob.gap = 6;
                            gui_button_state os = gui_button_begin(&ob);
                            gui_icon(GUI_ICON_FOLDER, 13, label_text.color);
                            gui_text("Open Project", &label_text);
                            gui_button_end();
                            if (os.clicked) {
                                picker->view = ED_PICKER_OPEN_PROJECT;
                                picker->error_msg[0] = '\0';
                                memset(&picker->path_input, 0, sizeof(picker->path_input));
                            }
                        }
                    }

                    // Recent projects
                    if (app->ed_cfg.recent_count > 0) {
                        gui_spacer_fixed(4);
                        gui_text("Recent Projects", &dim_text);
                        gui_separator();

                        for (u32 i = 0; i < app->ed_cfg.recent_count; i++) {
                            const char *path = app->ed_cfg.recent_projects[i];
                            if (!path[0]) continue;

                            gui_button_cfg row_btn = {
                                .hover_color = t->control_hover,
                                .press_color = t->control_press,
                                .padding = 6,
                                .corner_radius = 4,
                                .grow_width = true,
                                .align_left = true,
                                .no_bg = true,
                                .gap = 6,
                            };
                            gui_text_cfg path_text = dim_text;
                            path_text.max_chars = 52;
                            gui_button_state row_state = gui_button_begin(&row_btn);
                            gui_icon(GUI_ICON_FOLDER, 13, dim_text.color);
                            gui_text(path, &path_text);
                            gui_button_end();
                            if (row_state.clicked) {
                                picker_try_open(picker, app, path);
                                if (!picker->project_selected) {
                                    // Failed to open — remove from recents
                                    for (u32 j = i; j + 1 < app->ed_cfg.recent_count; j++) {
                                        cstr_copy(app->ed_cfg.recent_projects[j],
                                                  sizeof(app->ed_cfg.recent_projects[j]),
                                                  app->ed_cfg.recent_projects[j + 1]);
                                    }
                                    app->ed_cfg.recent_count--;
                                    app->ed_cfg.recent_projects[app->ed_cfg.recent_count][0] = '\0';
                                    ed_config_save(&app->ed_cfg);
                                }
                            }
                        }
                    }

                    if (picker->error_msg[0]) {
                        gui_text(picker->error_msg, &error_text);
                    }
                } break;

                case ED_PICKER_NEW_PROJECT: {
                    gui_text("New Project", &title_text);
                    gui_separator();

                    GUI_ROW(8) {
                        gui_text("Project Path:", &label_text);
                        gui_spacer();
                        {
                            gui_button_cfg bb = btn_cfg;
                            bb.gap = 6;
                            gui_button_state bs = gui_button_begin(&bb);
                            gui_icon(GUI_ICON_SEARCH, 13, label_text.color);
                            gui_text("Browse", &label_text);
                            gui_button_end();
                            if (bs.clicked) {
                                const char *init = picker->path_input.buf[0] ? picker->path_input.buf : nullptr;
                                gui_file_browser_open(&picker->file_browser,
                                                      GUI_FILE_BROWSER_DIRECTORY, init, nullptr);
                            }
                        }
                    }
                    {
                        b8 path_focused = gui_focus_is(picker->path_input._id);
                        gui_text_input_render(&picker->path_input,
                            path_focused ? dt : 0,
                            path_focused ? &input_cfg : &input_cfg_dim);
                    }

                    gui_text("Project Name:", &label_text);
                    {
                        b8 name_focused = gui_focus_is(picker->name_input._id);
                        gui_text_input_render(&picker->name_input,
                            name_focused ? dt : 0,
                            name_focused ? &input_cfg : &input_cfg_dim);
                    }

                    if (picker->error_msg[0]) {
                        gui_text(picker->error_msg, &error_text);
                    }

                    GUI_ROW(8) {
                        if (gui_text_button("Cancel", &btn_cfg, &label_text).clicked) {
                            picker->view = ED_PICKER_HOME;
                            picker->error_msg[0] = '\0';
                        }
                        {
                            gui_button_cfg cb = btn_cfg;
                            cb.gap = 6;
                            gui_button_state cs = gui_button_begin(&cb);
                            gui_icon(GUI_ICON_CHECK, 13, label_text.color);
                            gui_text("Create", &label_text);
                            gui_button_end();
                            if (cs.clicked) picker_try_create(picker, app);
                        }
                    }
                } break;

                case ED_PICKER_OPEN_PROJECT: {
                    gui_text("Open Project", &title_text);
                    gui_separator();

                    GUI_ROW(8) {
                        gui_text("Project Path:", &label_text);
                        gui_spacer();
                        {
                            gui_button_cfg bb = btn_cfg;
                            bb.gap = 6;
                            gui_button_state bs = gui_button_begin(&bb);
                            gui_icon(GUI_ICON_SEARCH, 13, label_text.color);
                            gui_text("Browse", &label_text);
                            gui_button_end();
                            if (bs.clicked) {
                                const char *init = picker->path_input.buf[0] ? picker->path_input.buf : nullptr;
                                gui_file_browser_open(&picker->file_browser,
                                                      GUI_FILE_BROWSER_DIRECTORY, init, nullptr);
                            }
                        }
                    }
                    gui_text_input_render(&picker->path_input, dt, &input_cfg);

                    if (picker->error_msg[0]) {
                        gui_text(picker->error_msg, &error_text);
                    }

                    GUI_ROW(8) {
                        if (gui_text_button("Cancel", &btn_cfg, &label_text).clicked) {
                            picker->view = ED_PICKER_HOME;
                            picker->error_msg[0] = '\0';
                        }
                        {
                            gui_button_cfg ob = btn_cfg;
                            ob.gap = 6;
                            gui_button_state os = gui_button_begin(&ob);
                            gui_icon(GUI_ICON_FOLDER, 13, label_text.color);
                            gui_text("Open", &label_text);
                            gui_button_end();
                            if (os.clicked) picker_try_open(picker, app, picker->path_input.buf);
                        }
                    }
                } break;
                }
            }
        }
    }
}

b8 ed_project_picker_handle_key(ed_project_picker *picker, void *key_data) {
    return on_key_press(key_data, picker);
}

b8 ed_project_picker_handle_char(ed_project_picker *picker, void *char_data) {
    return on_char_input(char_data, picker);
}

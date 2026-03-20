#include "core/ed_mode.h"

#include "core/ed_application.h"
#include "core/ed_mode_editor.h"
#include "core/ed_mode_picker.h"

typedef struct {
    void (*enter)(ed_application *app);
    void (*exit)(ed_application *app);
} ed_mode_def;

static const ed_mode_def modes[ED_MODE_COUNT] = {
    [ED_MODE_PICKER] = {ed_mode_picker_enter, ed_mode_picker_exit},
    [ED_MODE_EDITOR] = {ed_mode_editor_enter, ed_mode_editor_exit},
};

void ed_mode_switch(ed_application *app, ED_MODE new_mode) {
    ED_MODE old = app->mode;
    if (old == new_mode) return;

    // Exit current mode (skip if no mode was active yet)
    if (old < ED_MODE_COUNT && modes[old].exit) {
        modes[old].exit(app);
    }

    app->mode = new_mode;

    // Enter new mode (skip for ED_MODE_COUNT which means "no mode")
    if (new_mode < ED_MODE_COUNT && modes[new_mode].enter) {
        modes[new_mode].enter(app);
    }
}

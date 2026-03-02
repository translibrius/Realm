#include "app_toast.h"

#include "asset/asset.h"
#include "gui/gui_clay.h"
#include "memory/memory.h"
#include "util/str.h"

static const char *toast_type_tag(app_toast_type type) {
    switch (type) {
    case APP_TOAST_WARNING: return "WARN";
    case APP_TOAST_ERROR:   return "ERROR";
    case APP_TOAST_INFO:
    default:                return "INFO";
    }
}

static Clay_Color toast_type_color(app_toast_type type) {
    switch (type) {
    case APP_TOAST_WARNING: return GUI_RGBA(255, 191, 51, 255);
    case APP_TOAST_ERROR:   return GUI_RGBA(255, 89, 89, 255);
    case APP_TOAST_INFO:
    default:                return GUI_RGBA(128, 230, 255, 255);
    }
}

void app_toast_push(app_toast *toasts, app_toast_type type, const char *message) {
    if (!toasts || !message || !message[0]) {
        return;
    }

    for (u32 i = APP_MAX_TOASTS - 1; i > 0; i--) {
        toasts[i] = toasts[i - 1];
    }

    app_toast *toast = &toasts[0];
    mem_zero(toast, sizeof(*toast));
    cstr_copy(toast->message, sizeof(toast->message), message);
    toast->type = type;
    toast->active = true;

    switch (type) {
    case APP_TOAST_WARNING:
        toast->ttl_seconds = 3.5f;
        break;
    case APP_TOAST_ERROR:
        toast->ttl_seconds = 5.0f;
        break;
    case APP_TOAST_INFO:
    default:
        toast->ttl_seconds = 2.5f;
        break;
    }
}

void app_toast_update(app_toast *toasts, f64 dt) {
    if (!toasts) {
        return;
    }

    for (u32 i = 0; i < APP_MAX_TOASTS; i++) {
        app_toast *toast = &toasts[i];
        if (!toast->active) {
            continue;
        }

        toast->ttl_seconds -= (f32)dt;
        if (toast->ttl_seconds <= 0.0f) {
            toast->active = false;
        }
    }

    u32 write_index = 0;
    for (u32 read_index = 0; read_index < APP_MAX_TOASTS; read_index++) {
        if (!toasts[read_index].active) {
            continue;
        }

        if (write_index != read_index) {
            toasts[write_index] = toasts[read_index];
        }
        write_index++;
    }
    for (u32 i = write_index; i < APP_MAX_TOASTS; i++) {
        toasts[i].active = false;
    }
}

void app_toast_render(const app_toast *toasts) {
    if (!toasts) {
        return;
    }

    u32 active_count = 0;
    for (u32 i = 0; i < APP_MAX_TOASTS; i++) {
        if (toasts[i].active) {
            active_count++;
        }
    }
    if (active_count == 0) {
        return;
    }

    u16 font_jb = gui_font_id(ASSET_ID_FONT_JETBRAINS_MONO_REGULAR);

    gui_layout_begin(0.0f);

    Clay_ElementDeclaration root_decl = {
        .layout = GUI_ROOT_LAYOUT(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP),
    };
    Clay_ElementDeclaration col_decl = {
        .layout = {
            .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)},
            .padding = {.left = 8, .top = 28},
            .childGap = 2,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
    };

    CLAY(CLAY_ID("ToastRoot"), root_decl) {
        CLAY(CLAY_ID("ToastColumn"), col_decl) {
            for (u32 i = 0; i < APP_MAX_TOASTS; i++) {
                const app_toast *toast = &toasts[i];
                if (!toast->active) {
                    continue;
                }

                char buf[128];
                i32 len = cstr_format_buf(buf, sizeof(buf), "[%s] %s", toast_type_tag(toast->type), toast->message);
                Clay_String text = {.length = len, .chars = buf};
                Clay_Color color = toast_type_color(toast->type);

                CLAY_TEXT(text, GUI_TEXT_CFG_FONT(color, 16, font_jb));
            }
        }
    }

    gui_layout_end();
}

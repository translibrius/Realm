#include "platform/splash/splash.h"

#include "asset/asset_table.h"
#include "core/event.h"
#include "core/logger.h"
#include "memory/memory.h"
#include "platform/platform.h"

#include "splash_font.h"

#include <stdio.h>
#include <string.h>

typedef struct splash_screen {
    u32 pixels_size;
    u8 *pixels;
    u32 progress_step;
    const char *current_asset;
} splash_screen;

typedef struct rgba {
    u8 r, g, b, a;
} rgba;

static splash_screen state;
static b8 progress_registered = false;

b8 on_progress_increment(void *event, void *data) {
    (void)data;
    state.progress_step++;
    e_splash_payload *payload = event;
    if (payload) {
        state.current_asset = payload->asset_name;
    }
    return true;
}

// -------------------------------
// Pixel drawing primitives
// -------------------------------

static void splash_set_pixel(u32 x, u32 y, rgba color) {
    if (!state.pixels)
        return;
    if (x >= SPLASH_WIDTH || y >= SPLASH_HEIGHT)
        return;

    u32 index = (y * SPLASH_WIDTH + x) * 4;

    state.pixels[index + 0] = (u8)((color.b * color.a) / 255);
    state.pixels[index + 1] = (u8)((color.g * color.a) / 255);
    state.pixels[index + 2] = (u8)((color.r * color.a) / 255);
    state.pixels[index + 3] = color.a;
}

static void splash_fill_rect(u32 x, u32 y, u32 w, u32 h, rgba color) {
    for (u32 yy = y; yy < y + h; yy++) {
        for (u32 xx = x; xx < x + w; xx++) {
            splash_set_pixel(xx, yy, color);
        }
    }
}

static void splash_draw_hline(u32 x, u32 y, u32 w, rgba color) {
    for (u32 xx = x; xx < x + w; xx++) {
        splash_set_pixel(xx, y, color);
    }
}

// -------------------------------
// Bitmap font text drawing
// -------------------------------

static void splash_draw_char_scaled(u32 x, u32 y, char ch, rgba color, u32 scale) {
    if (ch < SPLASH_FONT_FIRST || ch > SPLASH_FONT_LAST) return;

    u32 glyph_index = (u32)(ch - SPLASH_FONT_FIRST) * SPLASH_FONT_HEIGHT;

    for (u32 row = 0; row < SPLASH_FONT_HEIGHT; row++) {
        u8 bits = splash_font_glyphs[glyph_index + row];
        for (u32 col = 0; col < SPLASH_FONT_WIDTH; col++) {
            if (bits & (0x80 >> col)) {
                splash_fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void splash_draw_text_scaled(u32 x, u32 y, const char *text, rgba color, u32 scale) {
    u32 char_w = SPLASH_FONT_WIDTH * scale;
    for (u32 i = 0; text[i]; i++) {
        splash_draw_char_scaled(x + i * char_w, y, text[i], color, scale);
    }
}

static u32 splash_text_width_scaled(const char *text, u32 scale) {
    u32 len = 0;
    while (text[len]) len++;
    return len * SPLASH_FONT_WIDTH * scale;
}

static void splash_draw_text_centered_scaled(u32 y, const char *text, rgba color, u32 scale) {
    u32 w = splash_text_width_scaled(text, scale);
    u32 x = (SPLASH_WIDTH > w) ? (SPLASH_WIDTH - w) / 2 : 0;
    splash_draw_text_scaled(x, y, text, color, scale);
}

// Convenience wrappers at default 2x scale
#define SPLASH_TEXT_SCALE  2
#define SPLASH_CHAR_W      (SPLASH_FONT_WIDTH  * SPLASH_TEXT_SCALE)
#define SPLASH_CHAR_H      (SPLASH_FONT_HEIGHT * SPLASH_TEXT_SCALE)

static void splash_draw_text(u32 x, u32 y, const char *text, rgba color) {
    splash_draw_text_scaled(x, y, text, color, SPLASH_TEXT_SCALE);
}

static u32 splash_text_width(const char *text) {
    return splash_text_width_scaled(text, SPLASH_TEXT_SCALE);
}


// -------------------------------
// Splash lifecycle
// -------------------------------

b8 splash_show() {
    if (!progress_registered) {
        event_register(EVENT_SPLASH_INCREMENT, on_progress_increment, nullptr);
        progress_registered = true;
    }
    state.progress_step = 0;
    state.current_asset = nullptr;
    state.pixels_size = SPLASH_WIDTH * SPLASH_HEIGHT * 4;
    state.pixels = mem_alloc(state.pixels_size, MEM_SUBSYSTEM_SPLASH);

    mem_zero(state.pixels, state.pixels_size);
    return platform_splash_create();
}

void splash_update() {
    // --- Catppuccin Mocha palette ---
    const rgba bg_color         = {30,  30,  46,  255};  // base
    const rgba surface0         = {49,  50,  68,  255};  // surface0
    const rgba overlay0         = {108, 112, 134, 255};  // overlay0
    const rgba subtext0         = {166, 173, 200, 255};  // subtext0
    const rgba green            = {166, 227, 161, 255};  // green
    const rgba lavender         = {180, 190, 254, 255};  // lavender

    // --- Layout constants ---
    constexpr u16 border_size     = 2;
    constexpr u16 margin          = 25;

    // Progress bar
    constexpr u16 bar_h           = 6;
    constexpr u16 bar_y_offset    = 50;  // from bottom

    // Title: "REALM" at 5x scale
    constexpr u32 title_scale     = 5;
    u32 title_h = SPLASH_FONT_HEIGHT * title_scale;

    // --- Clear background ---
    splash_fill_rect(0, 0, SPLASH_WIDTH, SPLASH_HEIGHT, bg_color);

    // --- Subtle border (2px) ---
    // Top
    splash_fill_rect(0, 0, SPLASH_WIDTH, border_size, surface0);
    // Left
    splash_fill_rect(0, border_size, border_size, SPLASH_HEIGHT - border_size * 2, surface0);
    // Bottom
    splash_fill_rect(0, SPLASH_HEIGHT - border_size, SPLASH_WIDTH, border_size, surface0);
    // Right
    splash_fill_rect(SPLASH_WIDTH - border_size, border_size, border_size, SPLASH_HEIGHT - border_size * 2, surface0);

    // --- "REALM" title centered in upper area ---
    u32 bar_area_top = SPLASH_HEIGHT - bar_y_offset;
    // Center title vertically in the space above the bar area, shifted up a bit
    u32 title_region_h = bar_area_top - SPLASH_CHAR_H - 20; // leave room for status text
    u32 title_y = (title_region_h > title_h) ? (title_region_h - title_h) / 2 : margin;
    splash_draw_text_centered_scaled(title_y, "REALM", lavender, title_scale);

    // --- Thin separator line below title ---
    u32 sep_y = title_y + title_h + 12;
    u32 sep_w = 200;
    u32 sep_x = (SPLASH_WIDTH - sep_w) / 2;
    splash_draw_hline(sep_x, sep_y, sep_w, surface0);

    // --- Progress bar (thin, modern) ---
    u32 bar_x = margin;
    u32 bar_w = SPLASH_WIDTH - margin * 2;
    u32 bar_y = SPLASH_HEIGHT - bar_y_offset;

    // Bar background track
    splash_fill_rect(bar_x, bar_y, bar_w, bar_h, surface0);

    // Bar fill
    u32 progress_w = (bar_w * state.progress_step) / ENGINE_ASSET_TABLE_COUNT;
    if (progress_w > 0) {
        splash_fill_rect(bar_x, bar_y, progress_w, bar_h, green);
    }

    // --- Status text: "Loading <filename>..." ---
    u32 status_y = bar_y - SPLASH_CHAR_H - 10;
    if (state.current_asset) {
        char status[128];
        snprintf(status, sizeof(status), "Loading %s...", state.current_asset);
        splash_draw_text(margin, status_y, status, subtext0);
    } else {
        splash_draw_text(margin, status_y, "Loading assets...", subtext0);
    }

    // --- Counter: "[N/Total]" right-aligned on same line ---
    char counter[32];
    snprintf(counter, sizeof(counter), "%u/%u", state.progress_step, (u32)ENGINE_ASSET_TABLE_COUNT);
    u32 counter_w = splash_text_width(counter);
    u32 counter_x = SPLASH_WIDTH - margin - counter_w;
    splash_draw_text(counter_x, status_y, counter, overlay0);

    // --- Version text, bottom-right corner ---
    const char *version = "v" REALM_VERSION;
    u32 ver_w = splash_text_width_scaled(version, 1);
    u32 ver_x = SPLASH_WIDTH - margin - ver_w;
    u32 ver_y = SPLASH_HEIGHT - border_size - SPLASH_FONT_HEIGHT - 6;
    splash_draw_text_scaled(ver_x, ver_y, version, overlay0, 1);

    platform_splash_update(state.pixels);
}

void splash_hide() {
    mem_free(state.pixels, state.pixels_size, MEM_SUBSYSTEM_SPLASH);
    platform_splash_destroy();
}

void splash_run(void *data) {
    (void)data;
    RL_DEBUG("Splash window spawned on thread: %d", platform_get_current_thread_id());
    if (!splash_show()) {
        RL_DEBUG("Failed to show splash window");
        return;
    }

    while (state.progress_step < ENGINE_ASSET_TABLE_COUNT) {
        splash_update();
    }

    splash_hide();
}

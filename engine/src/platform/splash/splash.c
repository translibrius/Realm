#include "platform/splash/splash.h"

#include "core/logger.h"
#include "memory/memory.h"
#include "util/str.h"

typedef struct splash_screen {
    u32 pixels_size;
    u8 *pixels;
    rl_arena *frame_arena;
} splash_screen;

typedef struct rgba {
    u8 r, g, b, a;
} rgba;

static splash_screen state;

// -------------------------------
// Basic pixel + rect drawing
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

// -------------------------------
// Splash lifecycle
// -------------------------------

b8 splash_show(rl_arena *frame_arena) {
    state.pixels_size = SPLASH_WIDTH * SPLASH_HEIGHT * 4;
    state.pixels = rl_alloc(state.pixels_size, MEM_SUBSYSTEM_SPLASH);
    state.frame_arena = frame_arena;

    rl_zero(state.pixels, state.pixels_size);
    return platform_splash_create();
}

void splash_update() {
    // Background
    const rgba bg_color = {30, 30, 46, 255};
    splash_fill_rect(0, 0, SPLASH_WIDTH, SPLASH_HEIGHT, bg_color);

    // Border
    const rgba border_color = {49, 50, 68, 255};
    constexpr u16 border_size = 10;
    // Top
    splash_fill_rect(0, 0, SPLASH_WIDTH, border_size, border_color);
    // Left
    splash_fill_rect(0, border_size, border_size, SPLASH_HEIGHT - border_size, border_color);
    // Bottom
    splash_fill_rect(0, SPLASH_HEIGHT - border_size, SPLASH_WIDTH, border_size, border_color);
    // Right
    splash_fill_rect(SPLASH_WIDTH - border_size, border_size, border_size, SPLASH_HEIGHT - border_size, border_color);

    constexpr u16 padding = border_size + 15;

    // Progress bar
    rgba progress_color = {69, 71, 90, 255};
    u16 progress_bar_height = 40;
    splash_fill_rect(padding, SPLASH_HEIGHT - progress_bar_height - padding, SPLASH_WIDTH - padding * 2, progress_bar_height, progress_color);

    // Calculate LOGO, which is just text "REALM" available width
    //const u16 logo_width = SPLASH_WIDTH - padding * 2;
    //const u16 logo_height = SPLASH_HEIGHT - padding * 2 - progress_bar_height;

    platform_splash_update(state.pixels);
}

void splash_hide() {
    rl_free(state.pixels, state.pixels_size, MEM_SUBSYSTEM_SPLASH);
}

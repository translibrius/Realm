#include "platform/splash/splash.h"

#include "core/logger.h"
#include "memory/memory.h"
#include "util/clock.h"
#include "util/str.h"

typedef struct splash_screen {
    u32 pixels_size;
    u8 *pixels;

    rl_clock progress_clock;
    u32 progress_step;
} splash_screen;

typedef struct rgba {
    u8 r, g, b, a;
} rgba;

static splash_screen state;

#define PROGRESS_MAX_STEPS 250

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

b8 splash_show() {
    clock_reset(&state.progress_clock);
    state.progress_step = 0;
    state.pixels_size = SPLASH_WIDTH * SPLASH_HEIGHT * 4;
    state.pixels = rl_alloc(state.pixels_size, MEM_SUBSYSTEM_SPLASH);

    rl_zero(state.pixels, state.pixels_size);
    return platform_splash_create();
}

void splash_update() {
    clock_update(&state.progress_clock);

    // Advance progress every 0.2 sec
    if (clock_elapsed_s(&state.progress_clock) > 0.004166) {
        state.progress_step++;
        if (state.progress_step > PROGRESS_MAX_STEPS) {
            state.progress_step = 0;
        }
        clock_reset(&state.progress_clock);
    }

    // Colors
    const rgba bg_color = {30, 30, 46, 255};
    const rgba border_color = {49, 50, 68, 255};
    const rgba bar_border_color = {69, 71, 90, 255};
    const rgba bar_fill_color = {166, 227, 161, 255};

    constexpr u16 border_size = 10;
    constexpr u16 padding = border_size + 15;
    constexpr u16 bar_border_h = 40;
    constexpr u16 bar_inner_padding = 3;
    constexpr u16 bar_h = 34;

    // Clear background
    splash_fill_rect(0, 0, SPLASH_WIDTH, SPLASH_HEIGHT, bg_color);

    // Border
    splash_fill_rect(0, 0, SPLASH_WIDTH, border_size, border_color); // top
    splash_fill_rect(0, border_size, border_size, SPLASH_HEIGHT - border_size, border_color); // left
    splash_fill_rect(0, SPLASH_HEIGHT - border_size, SPLASH_WIDTH, border_size, border_color); // bottom
    splash_fill_rect(SPLASH_WIDTH - border_size, border_size, border_size, SPLASH_HEIGHT - border_size, border_color); // right

    // Progress bar border
    u32 border_x = padding;
    u32 border_y = SPLASH_HEIGHT - bar_border_h - padding;
    u32 border_w = SPLASH_WIDTH - padding * 2;
    splash_fill_rect(border_x, border_y, border_w, bar_border_h, bar_border_color);

    // Inner progress bar area
    u32 inner_x = border_x + bar_inner_padding;
    u32 inner_y = border_y + bar_inner_padding;
    u32 inner_w = border_w - bar_inner_padding * 2;

    // Compute progress width safely
    u32 progress_w = (inner_w * state.progress_step) / PROGRESS_MAX_STEPS;

    // Fill progress
    splash_fill_rect(inner_x, inner_y, progress_w, bar_h, bar_fill_color);

    platform_splash_update(state.pixels);
}


void splash_hide() {
    rl_free(state.pixels, state.pixels_size, MEM_SUBSYSTEM_SPLASH);
    platform_splash_destroy();
}

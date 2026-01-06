#include "gui/gui.h"

#define CLAY_IMPLEMENTATION
#include "../vendor/clay/clay.h"
#include "core/logger.h"
#include "memory/memory.h"

void clay_error_handler(Clay_ErrorData error_data) {
    RL_ERROR("GUI Error (Clay): %s", error_data.errorText.chars);
}

void init_gui(f32 width, f32 height) {
    u64 clay_memory_size = Clay_MinMemorySize();
    Clay_Arena ui_arena = Clay_CreateArenaWithCapacityAndMemory(clay_memory_size, rl_alloc(clay_memory_size, MEM_SUBSYSTEM_GUI));

    Clay_Initialize(ui_arena, (Clay_Dimensions){width, height}, (Clay_ErrorHandler){clay_error_handler});

    RL_INFO("Successfully initialize GUI Subsystem");
}
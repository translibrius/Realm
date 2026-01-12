#include "platform/platform.h"

void log_system_info() {
    platform_info *info = platform_get_info();
    RL_DEBUG("System details: ");
    RL_DEBUG("----------------------------");
    RL_DEBUG("Operating system: Windows | Build: %d | Version: %d.%d", info->build_number, info->version_major, info->version_minor);
    RL_DEBUG("Arch: %s", info->arch);
    RL_DEBUG("Page size: %d", info->page_size);
    RL_DEBUG("Logical processors: %d", info->logical_processors);
    RL_DEBUG("Allocation granularity: %d", info->alloc_granularity);
    RL_DEBUG("Clock frequency: %d", info->clock_freq);
}
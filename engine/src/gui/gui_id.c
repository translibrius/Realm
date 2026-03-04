#include "gui_internal.h"

// Single shared counter across all widget types.
// Jenkins hash with a high seed avoids collisions with Clay's auto-generated IDs.
static u32 counter;

u32 gui__next_id(void) {
    counter++;
    u32 hash = 0x47554900; // "GUI\0" seed
    hash += (counter + 48);
    hash += (hash << 10);
    hash ^= (hash >> 6);
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash + 1;
}

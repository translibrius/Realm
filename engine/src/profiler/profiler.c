#include "profiler/profiler.h"

#if RL_PROFILE_ENABLED

#include "core/logger.h"
#include "memory/memory.h"
#include "platform/platform.h"

#include <stdio.h>
#include <string.h>

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
#include <pthread.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#if defined(PLATFORM_WINDOWS)
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

// Every function in this file must never be instrumented.
#define PROF_NOINST __attribute__((no_instrument_function))

// Thread-safe thread ID check (must not be instrumented, so use OS call directly)
PROF_NOINST
static u64 profiler_get_thread_id(void) {
#if defined(PLATFORM_WINDOWS)
    return (u64)GetCurrentThreadId();
#else
    return (u64)pthread_self();
#endif
}

#define AGG_MAP_SIZE    4096
#define NAME_MAP_SIZE   4096
#define MAX_ZONES       1024
#define MAX_STACK_DEPTH 256
#define UPDATE_INTERVAL_FRAMES 6  // Update display snapshot every N frames (~10 Hz at 60fps)

// --- Shared memory broadcast layout ---
#define SHM_NAME            "/realm_profiler"
#define SHM_NAME_WIN        "realm_profiler"
#define MAX_BROADCAST_ZONES 64
#define SHM_ZONE_NAME_LEN   32

typedef struct shm_zone {
    char name[SHM_ZONE_NAME_LEN];
    u32  call_count;
    i64  total_ns;
    i64  max_ns;
    i64  avg_ns;
} shm_zone;

typedef struct shm_header {
    u32      magic;         // 0x524C5046 = "RLPF"
    u32      sequence;      // monotonically increasing
    u32      zone_count;
    u32      _pad;
    i64      frame_time_ns;
    shm_zone zones[MAX_BROADCAST_ZONES];
} shm_header;

#define SHM_SIZE sizeof(shm_header)

// --- Internal types ---

typedef struct agg_entry {
    void *fn_addr;
    u32   call_count;
    i64   total_ns;
    i64   max_ns;
} agg_entry;

typedef struct name_entry {
    void       *fn_addr;
    const char *name;
} name_entry;

typedef struct timing_entry {
    void *fn_addr;
    i64   enter_time;
} timing_entry;

typedef struct profiler_state {
    // Per-frame raw aggregation (cleared every frame)
    agg_entry       agg_map[AGG_MAP_SIZE];

    // Multi-frame accumulation (cleared every UPDATE_INTERVAL_FRAMES)
    agg_entry       accum_map[AGG_MAP_SIZE];
    u32             accum_frames;  // how many frames accumulated so far

    name_entry      name_map[NAME_MAP_SIZE];

    rl_profile_zone zone_storage[MAX_ZONES];
    rl_profile_frame completed_frame;

    timing_entry    call_stack[MAX_STACK_DEPTH];
    u32             stack_depth;

    i64             frame_start_time;
    i64             clock_freq;
    i64             accum_frame_ns;  // accumulated frame time over interval

    // Lifetime accumulation (never reset — for session report on exit)
    agg_entry       lifetime_map[AGG_MAP_SIZE];
    u32             lifetime_frames;
    i64             lifetime_total_ns;

    // Shared memory
    shm_header     *shm_ptr;
    u32             shm_sequence;
#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    i32             shm_fd;
#endif
#if defined(PLATFORM_WINDOWS)
    void           *shm_handle;
#endif

    b8              enabled;
    b8              initialized;
    u64             main_thread_id;
} profiler_state;

static profiler_state state;

// --- Hash helper ---

PROF_NOINST
static u32 hash_ptr(void *ptr) {
    return (u32)((u64)ptr * 2654435761ULL) & (AGG_MAP_SIZE - 1);
}

PROF_NOINST
static u32 hash_ptr_name(void *ptr) {
    return (u32)((u64)ptr * 2654435761ULL) & (NAME_MAP_SIZE - 1);
}

// --- Name resolution ---

PROF_NOINST
static const char *resolve_name(void *fn_addr) {
    u32 hash = hash_ptr_name(fn_addr);
    for (u32 probe = 0; probe < NAME_MAP_SIZE; probe++) {
        u32 slot = (hash + probe) & (NAME_MAP_SIZE - 1);
        if (state.name_map[slot].fn_addr == fn_addr) {
            return state.name_map[slot].name;
        }
        if (state.name_map[slot].fn_addr == nullptr) {
            break;
        }
    }

    const char *name = "???";

#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    Dl_info info;
    if (dladdr(fn_addr, &info) && info.dli_sname) {
        u32 len = (u32)strlen(info.dli_sname);
        char *buf = mem_alloc(len + 1, MEM_SUBSYSTEM_PROFILER);
        memcpy(buf, info.dli_sname, len + 1);
        name = buf;
    }
#elif defined(PLATFORM_WINDOWS)
    char sym_buf[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    SYMBOL_INFO *sym = (SYMBOL_INFO *)sym_buf;
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = MAX_SYM_NAME;
    DWORD64 displacement = 0;
    if (SymFromAddr(GetCurrentProcess(), (DWORD64)fn_addr, &displacement, sym)) {
        u32 len = (u32)sym->NameLen;
        char *buf = mem_alloc(len + 1, MEM_SUBSYSTEM_PROFILER);
        memcpy(buf, sym->Name, len + 1);
        name = buf;
    }
#endif

    // Insert into name cache
    hash = hash_ptr_name(fn_addr);
    for (u32 probe = 0; probe < NAME_MAP_SIZE; probe++) {
        u32 slot = (hash + probe) & (NAME_MAP_SIZE - 1);
        if (state.name_map[slot].fn_addr == nullptr) {
            state.name_map[slot].fn_addr = fn_addr;
            state.name_map[slot].name    = name;
            return name;
        }
    }
    return name;
}

// --- Shared memory helpers ---

PROF_NOINST
static void shm_create(void) {
#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    // Clean up any stale shm from a previous crash
    shm_unlink(SHM_NAME);

    state.shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (state.shm_fd < 0) {
        RL_WARN("Profiler: failed to create shared memory");
        return;
    }
    if (ftruncate(state.shm_fd, (off_t)SHM_SIZE) != 0) {
        RL_WARN("Profiler: failed to resize shared memory");
        close(state.shm_fd);
        shm_unlink(SHM_NAME);
        state.shm_fd = -1;
        return;
    }
    state.shm_ptr = (shm_header *)mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, state.shm_fd, 0);
    if (state.shm_ptr == MAP_FAILED) {
        RL_WARN("Profiler: failed to mmap shared memory");
        close(state.shm_fd);
        shm_unlink(SHM_NAME);
        state.shm_fd = -1;
        state.shm_ptr = nullptr;
        return;
    }
    memset(state.shm_ptr, 0, SHM_SIZE);
    state.shm_ptr->magic = 0x524C5046; // "RLPF"
#endif

#if defined(PLATFORM_WINDOWS)
    state.shm_handle = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                           0, (DWORD)SHM_SIZE, SHM_NAME_WIN);
    if (!state.shm_handle) {
        RL_WARN("Profiler: failed to create shared memory");
        return;
    }
    state.shm_ptr = (shm_header *)MapViewOfFile(state.shm_handle, FILE_MAP_ALL_ACCESS,
                                                  0, 0, SHM_SIZE);
    if (!state.shm_ptr) {
        RL_WARN("Profiler: failed to map shared memory");
        CloseHandle(state.shm_handle);
        state.shm_handle = NULL;
        return;
    }
    memset(state.shm_ptr, 0, SHM_SIZE);
    state.shm_ptr->magic = 0x524C5046; // "RLPF"
#endif
}

PROF_NOINST
static void shm_destroy(void) {
#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    if (state.shm_ptr && state.shm_ptr != MAP_FAILED) {
        // Signal viewers that we're shutting down
        state.shm_ptr->magic = 0;
        munmap(state.shm_ptr, SHM_SIZE);
    }
    if (state.shm_fd >= 0) {
        close(state.shm_fd);
        shm_unlink(SHM_NAME);
    }
    state.shm_ptr = nullptr;
    state.shm_fd = -1;
#endif

#if defined(PLATFORM_WINDOWS)
    if (state.shm_ptr) {
        state.shm_ptr->magic = 0;
        UnmapViewOfFile(state.shm_ptr);
    }
    if (state.shm_handle) {
        CloseHandle(state.shm_handle);
    }
    state.shm_ptr = nullptr;
    state.shm_handle = NULL;
#endif
}

PROF_NOINST
static void shm_broadcast(void) {
    if (!state.shm_ptr) return;

    const rl_profile_frame *frame = &state.completed_frame;
    u32 count = frame->zone_count;
    if (count > MAX_BROADCAST_ZONES) count = MAX_BROADCAST_ZONES;

    // Write data first, then update sequence last (poor-man's publish barrier)
    state.shm_ptr->frame_time_ns = frame->frame_time_ns;
    state.shm_ptr->zone_count = count;

    for (u32 i = 0; i < count; i++) {
        const rl_profile_zone *z = &frame->zones[i];
        shm_zone *dst = &state.shm_ptr->zones[i];

        // Copy name, truncate to fit
        memset(dst->name, 0, SHM_ZONE_NAME_LEN);
        if (z->name) {
            u32 len = (u32)strlen(z->name);
            if (len >= SHM_ZONE_NAME_LEN) len = SHM_ZONE_NAME_LEN - 1;
            memcpy(dst->name, z->name, len);
        }
        dst->call_count = z->call_count;
        dst->total_ns   = z->total_ns;
        dst->max_ns     = z->max_ns;
        dst->avg_ns     = z->avg_ns;
    }

    // Increment sequence last — acts as a release fence for the viewer
    state.shm_sequence++;
    state.shm_ptr->sequence = state.shm_sequence;
}

// --- Accumulation helper ---

PROF_NOINST
static void accum_merge_frame(void) {
    // Merge this frame's raw agg_map into the multi-frame accum_map
    for (u32 i = 0; i < AGG_MAP_SIZE; i++) {
        agg_entry *src = &state.agg_map[i];
        if (src->fn_addr == nullptr) continue;

        u32 hash = hash_ptr(src->fn_addr);
        for (u32 probe = 0; probe < AGG_MAP_SIZE; probe++) {
            u32 slot = (hash + probe) & (AGG_MAP_SIZE - 1);
            agg_entry *dst = &state.accum_map[slot];

            if (dst->fn_addr == nullptr) {
                *dst = *src;
                break;
            }
            if (dst->fn_addr == src->fn_addr) {
                dst->call_count += src->call_count;
                dst->total_ns  += src->total_ns;
                if (src->max_ns > dst->max_ns) dst->max_ns = src->max_ns;
                break;
            }
        }
    }
}

PROF_NOINST
static void lifetime_merge_frame(void) {
    // Merge this frame's raw agg_map into the lifetime map (never reset)
    for (u32 i = 0; i < AGG_MAP_SIZE; i++) {
        agg_entry *src = &state.agg_map[i];
        if (src->fn_addr == nullptr) continue;

        u32 hash = hash_ptr(src->fn_addr);
        for (u32 probe = 0; probe < AGG_MAP_SIZE; probe++) {
            u32 slot = (hash + probe) & (AGG_MAP_SIZE - 1);
            agg_entry *dst = &state.lifetime_map[slot];

            if (dst->fn_addr == nullptr) {
                *dst = *src;
                break;
            }
            if (dst->fn_addr == src->fn_addr) {
                dst->call_count += src->call_count;
                dst->total_ns  += src->total_ns;
                if (src->max_ns > dst->max_ns) dst->max_ns = src->max_ns;
                break;
            }
        }
    }
}

// --- Lifecycle ---

PROF_NOINST
void rl_profiler_init(void) {
    memset(&state, 0, sizeof(state));
    state.clock_freq = platform_get_info()->clock_freq;
    state.frame_start_time = platform_get_clock_counter();
#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    state.shm_fd = -1;
#endif
    state.main_thread_id = profiler_get_thread_id();
    state.enabled = true;
    state.initialized = true;

#if defined(PLATFORM_WINDOWS)
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
#endif

    shm_create();
    RL_INFO("Profiler initialized (shared memory broadcast active)");
}

PROF_NOINST
void rl_profiler_shutdown(void) {
    if (!state.initialized) return;
    shm_destroy();
#if defined(PLATFORM_WINDOWS)
    SymCleanup(GetCurrentProcess());
#endif
    state.initialized = false;
    RL_INFO("Profiler shut down");
}

// --- Instrumentation hooks ---

PROF_NOINST
REALM_API void __cyg_profile_func_enter(void *fn, void *caller) {
    (void)caller;
    if (!state.enabled || !state.initialized) return;
    if (profiler_get_thread_id() != state.main_thread_id) return;
    if (state.stack_depth >= MAX_STACK_DEPTH) return;

    state.call_stack[state.stack_depth] = (timing_entry){
        .fn_addr    = fn,
        .enter_time = platform_get_clock_counter(),
    };
    state.stack_depth++;
}

PROF_NOINST
REALM_API void __cyg_profile_func_exit(void *fn, void *caller) {
    (void)caller;
    if (!state.enabled || !state.initialized) return;
    if (profiler_get_thread_id() != state.main_thread_id) return;
    if (state.stack_depth == 0) return;
    if (state.call_stack[state.stack_depth - 1].fn_addr != fn) return;

    state.stack_depth--;
    i64 now = platform_get_clock_counter();
    i64 elapsed = now - state.call_stack[state.stack_depth].enter_time;
    i64 elapsed_ns = (elapsed * 1000000000LL) / state.clock_freq;

    u32 hash = hash_ptr(fn);
    for (u32 probe = 0; probe < AGG_MAP_SIZE; probe++) {
        u32 slot = (hash + probe) & (AGG_MAP_SIZE - 1);
        agg_entry *e = &state.agg_map[slot];

        if (e->fn_addr == nullptr) {
            e->fn_addr    = fn;
            e->call_count = 1;
            e->total_ns   = elapsed_ns;
            e->max_ns     = elapsed_ns;
            return;
        }
        if (e->fn_addr == fn) {
            e->call_count++;
            e->total_ns += elapsed_ns;
            if (elapsed_ns > e->max_ns) e->max_ns = elapsed_ns;
            return;
        }
    }
}

// --- Frame mark + aggregation ---

PROF_NOINST
void rl_profiler_frame_mark(void) {
    if (!state.initialized) return;

    i64 now = platform_get_clock_counter();
    i64 frame_ns = ((now - state.frame_start_time) * 1000000000LL) / state.clock_freq;
    state.frame_start_time = now;

    // Merge this frame's data into both accumulators
    accum_merge_frame();
    lifetime_merge_frame();
    state.accum_frame_ns += frame_ns;
    state.accum_frames++;
    state.lifetime_total_ns += frame_ns;
    state.lifetime_frames++;

    // Clear per-frame raw map
    memset(state.agg_map, 0, sizeof(state.agg_map));
    state.stack_depth = 0;

    // Only update the display snapshot every N frames
    if (state.accum_frames < UPDATE_INTERVAL_FRAMES) return;

    u32 num_frames = state.accum_frames;
    i64 avg_frame_ns = state.accum_frame_ns / (i64)num_frames;

    // Build sorted snapshot from accumulated data, averaging per frame
    u32 count = 0;
    for (u32 i = 0; i < AGG_MAP_SIZE && count < MAX_ZONES; i++) {
        agg_entry *e = &state.accum_map[i];
        if (e->fn_addr == nullptr) continue;

        const char *name = resolve_name(e->fn_addr);
        i64 avg_total_ns = e->total_ns / (i64)num_frames;
        u32 avg_calls    = (e->call_count + num_frames / 2) / num_frames;
        i64 avg_per_call = avg_calls > 0 ? avg_total_ns / (i64)avg_calls : 0;

        // Skip negligible entries
        if (avg_total_ns < 100) continue;

        state.zone_storage[count] = (rl_profile_zone){
            .name       = name,
            .fn_addr    = e->fn_addr,
            .call_count = avg_calls,
            .total_ns   = avg_total_ns,
            .max_ns     = e->max_ns,
            .avg_ns     = avg_per_call,
        };
        count++;
    }

    // Insertion sort by total_ns descending
    for (u32 i = 1; i < count; i++) {
        rl_profile_zone tmp = state.zone_storage[i];
        u32 j = i;
        while (j > 0 && state.zone_storage[j - 1].total_ns < tmp.total_ns) {
            state.zone_storage[j] = state.zone_storage[j - 1];
            j--;
        }
        state.zone_storage[j] = tmp;
    }

    state.completed_frame = (rl_profile_frame){
        .zones         = state.zone_storage,
        .zone_count    = count,
        .frame_time_ns = avg_frame_ns,
    };

    // Broadcast to shared memory for external viewer
    shm_broadcast();

    // Reset accumulator for next interval
    memset(state.accum_map, 0, sizeof(state.accum_map));
    state.accum_frames = 0;
    state.accum_frame_ns = 0;
}

PROF_NOINST
const rl_profile_frame *rl_profiler_get_frame(void) {
    return &state.completed_frame;
}

PROF_NOINST
void rl_profiler_set_enabled(b8 enabled) {
    if (!state.initialized) return;
    state.enabled = enabled;
    if (!enabled) {
        // Clear the call stack so we don't have stale half-entries
        state.stack_depth = 0;
    }
}

PROF_NOINST
b8 rl_profiler_is_enabled(void) {
    return state.enabled;
}

// --- Session report (binary file, same layout as shm_header) ---

PROF_NOINST
void rl_profiler_write_session_report(const char *path) {
    if (!state.initialized || !path) return;
    if (state.lifetime_frames == 0) return;

    // Build sorted zone list from lifetime data
    rl_profile_zone zones[MAX_BROADCAST_ZONES];
    u32 count = 0;
    u32 num_frames = state.lifetime_frames;

    for (u32 i = 0; i < AGG_MAP_SIZE && count < MAX_BROADCAST_ZONES; i++) {
        agg_entry *e = &state.lifetime_map[i];
        if (e->fn_addr == nullptr) continue;

        const char *name = resolve_name(e->fn_addr);
        i64 avg_total_ns = e->total_ns / (i64)num_frames;
        u32 avg_calls    = (e->call_count + num_frames / 2) / num_frames;
        i64 avg_per_call = avg_calls > 0 ? avg_total_ns / (i64)avg_calls : 0;

        if (avg_total_ns < 100) continue;

        zones[count] = (rl_profile_zone){
            .name       = name,
            .fn_addr    = e->fn_addr,
            .call_count = avg_calls,
            .total_ns   = avg_total_ns,
            .max_ns     = e->max_ns,
            .avg_ns     = avg_per_call,
        };
        count++;
    }

    // Sort by total_ns descending
    for (u32 i = 1; i < count; i++) {
        rl_profile_zone tmp = zones[i];
        u32 j = i;
        while (j > 0 && zones[j - 1].total_ns < tmp.total_ns) {
            zones[j] = zones[j - 1];
            j--;
        }
        zones[j] = tmp;
    }

    // Write as shm_header format (readable by profiler_report.py)
    FILE *f = fopen(path, "wb");
    if (!f) {
        RL_WARN("Profiler: failed to write session report to %s", path);
        return;
    }

    i64 avg_frame_ns = state.lifetime_total_ns / (i64)num_frames;

    shm_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic         = 0x524C5046;
    hdr.sequence      = num_frames;  // repurpose as frame count for session reports
    hdr.zone_count    = count;
    hdr.frame_time_ns = avg_frame_ns;

    for (u32 i = 0; i < count; i++) {
        memset(hdr.zones[i].name, 0, SHM_ZONE_NAME_LEN);
        if (zones[i].name) {
            u32 len = (u32)strlen(zones[i].name);
            if (len >= SHM_ZONE_NAME_LEN) len = SHM_ZONE_NAME_LEN - 1;
            memcpy(hdr.zones[i].name, zones[i].name, len);
        }
        hdr.zones[i].call_count = zones[i].call_count;
        hdr.zones[i].total_ns   = zones[i].total_ns;
        hdr.zones[i].max_ns     = zones[i].max_ns;
        hdr.zones[i].avg_ns     = zones[i].avg_ns;
    }

    fwrite(&hdr, sizeof(hdr), 1, f);
    fclose(f);
    RL_INFO("Profiler: session report written to %s (%u frames, %u zones)", path, num_frames, count);
}

#endif // RL_PROFILE_ENABLED

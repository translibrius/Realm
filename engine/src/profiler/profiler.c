#include "profiler/profiler.h"

#if RL_PROFILE_ENABLED

#include "core/logger.h"
#include "memory/memory.h"
#include "platform/platform.h"
#include "util/str.h"

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

// --- Shared memory broadcast layout (flat zones) ---
#define SHM_NAME            "/realm_profiler"
#define SHM_NAME_WIN        "realm_profiler"
#define MAX_BROADCAST_ZONES 64
#define SHM_ZONE_NAME_LEN   32

// --- Shared memory broadcast layout (call tree edges) ---
#define SHM_TREE_NAME       "/realm_profiler_tree"
#define SHM_TREE_NAME_WIN   "realm_profiler_tree"
#define MAX_BROADCAST_EDGES 256
#define SHM_EDGE_NAME_LEN   32

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

typedef struct shm_edge {
    char parent_name[SHM_EDGE_NAME_LEN];
    char child_name[SHM_EDGE_NAME_LEN];
    u32  call_count;
    u32  _pad;
    i64  total_ns;
    i64  max_ns;
    i64  avg_ns;
} shm_edge;

typedef struct shm_tree_header {
    u32      magic;         // 0x524C5054 = "RLPT"
    u32      sequence;      // matches flat segment's sequence
    u32      edge_count;
    u32      _pad;
    i64      frame_time_ns;
    shm_edge edges[MAX_BROADCAST_EDGES];
} shm_tree_header;

#define SHM_TREE_SIZE sizeof(shm_tree_header)

// --- Internal types ---

#define EDGE_MAP_SIZE 4096

typedef struct edge_entry {
    void *parent_addr;
    void *child_addr;
    u32   call_count;
    u32   _pad;
    i64   total_ns;
    i64   max_ns;
} edge_entry;

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

    // Parent-child edge aggregation
    edge_entry      edge_map[EDGE_MAP_SIZE];          // per-frame
    edge_entry      edge_accum_map[EDGE_MAP_SIZE];    // multi-frame batch
    edge_entry      edge_lifetime_map[EDGE_MAP_SIZE]; // session-long

    // Lifetime accumulation (never reset — for session report on exit)
    agg_entry       lifetime_map[AGG_MAP_SIZE];
    u32             lifetime_frames;
    i64             lifetime_total_ns;

    // Shared memory (flat zones)
    shm_header     *shm_ptr;
    u32             shm_sequence;

    // Shared memory (call tree edges)
    shm_tree_header *shm_tree_ptr;
#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    i32             shm_tree_fd;
#endif
#if defined(PLATFORM_WINDOWS)
    void           *shm_tree_handle;
#endif
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

PROF_NOINST
static u32 hash_edge(void *parent, void *child) {
    u64 combined = (u64)parent * 2654435761ULL ^ (u64)child * 2246822519ULL;
    return (u32)combined & (EDGE_MAP_SIZE - 1);
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
        u32 len = cstr_len(info.dli_sname);
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

// --- Shared memory helpers (call tree edges) ---

PROF_NOINST
static void shm_tree_create(void) {
#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    shm_unlink(SHM_TREE_NAME);

    state.shm_tree_fd = shm_open(SHM_TREE_NAME, O_CREAT | O_RDWR, 0666);
    if (state.shm_tree_fd < 0) {
        RL_WARN("Profiler: failed to create tree shared memory");
        return;
    }
    if (ftruncate(state.shm_tree_fd, (off_t)SHM_TREE_SIZE) != 0) {
        RL_WARN("Profiler: failed to resize tree shared memory");
        close(state.shm_tree_fd);
        shm_unlink(SHM_TREE_NAME);
        state.shm_tree_fd = -1;
        return;
    }
    state.shm_tree_ptr = (shm_tree_header *)mmap(nullptr, SHM_TREE_SIZE, PROT_READ | PROT_WRITE,
                                                   MAP_SHARED, state.shm_tree_fd, 0);
    if (state.shm_tree_ptr == MAP_FAILED) {
        RL_WARN("Profiler: failed to mmap tree shared memory");
        close(state.shm_tree_fd);
        shm_unlink(SHM_TREE_NAME);
        state.shm_tree_fd = -1;
        state.shm_tree_ptr = nullptr;
        return;
    }
    memset(state.shm_tree_ptr, 0, SHM_TREE_SIZE);
    state.shm_tree_ptr->magic = 0x524C5054; // "RLPT"
#endif

#if defined(PLATFORM_WINDOWS)
    state.shm_tree_handle = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                                0, (DWORD)SHM_TREE_SIZE, SHM_TREE_NAME_WIN);
    if (!state.shm_tree_handle) {
        RL_WARN("Profiler: failed to create tree shared memory");
        return;
    }
    state.shm_tree_ptr = (shm_tree_header *)MapViewOfFile(state.shm_tree_handle, FILE_MAP_ALL_ACCESS,
                                                            0, 0, SHM_TREE_SIZE);
    if (!state.shm_tree_ptr) {
        RL_WARN("Profiler: failed to map tree shared memory");
        CloseHandle(state.shm_tree_handle);
        state.shm_tree_handle = NULL;
        return;
    }
    memset(state.shm_tree_ptr, 0, SHM_TREE_SIZE);
    state.shm_tree_ptr->magic = 0x524C5054; // "RLPT"
#endif
}

PROF_NOINST
static void shm_tree_destroy(void) {
#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    if (state.shm_tree_ptr && state.shm_tree_ptr != MAP_FAILED) {
        state.shm_tree_ptr->magic = 0;
        munmap(state.shm_tree_ptr, SHM_TREE_SIZE);
    }
    if (state.shm_tree_fd >= 0) {
        close(state.shm_tree_fd);
        shm_unlink(SHM_TREE_NAME);
    }
    state.shm_tree_ptr = nullptr;
    state.shm_tree_fd = -1;
#endif

#if defined(PLATFORM_WINDOWS)
    if (state.shm_tree_ptr) {
        state.shm_tree_ptr->magic = 0;
        UnmapViewOfFile(state.shm_tree_ptr);
    }
    if (state.shm_tree_handle) {
        CloseHandle(state.shm_tree_handle);
    }
    state.shm_tree_ptr = nullptr;
    state.shm_tree_handle = NULL;
#endif
}

PROF_NOINST
static void shm_copy_name(char *dst, u32 dst_size, const char *src) {
    memset(dst, 0, dst_size);
    if (src) {
        u32 len = cstr_len(src);
        if (len >= dst_size) len = dst_size - 1;
        memcpy(dst, src, len);
    }
}

// --- Vendor noise filter ---
// Filters out high-frequency trivial vendor functions from the live display
// so they don't consume broadcast slots. Session reports still include them.

static const char *vendor_prefixes[] = {
    "Clay__",    // Clay internal helpers (Array_RangeCheck, ElementHasConfig, ...)
    "Clay_Layout",
    "Clay_Render",
    "glm_",      // cglm math (translate, mat4_identity, lookat, ...)
    "glmm_",     // cglm SIMD helpers (fmadd, ...)
    "vdupq_",    // ARM NEON intrinsics
    "veorq_",
    "vfmaq_",
    "vcombine_",
    "vdup_n_",
};

#define VENDOR_PREFIX_COUNT (sizeof(vendor_prefixes) / sizeof(vendor_prefixes[0]))

PROF_NOINST
static b8 profiler_is_vendor_noise(const char *name) {
    for (u32 i = 0; i < VENDOR_PREFIX_COUNT; i++) {
        const char *prefix = vendor_prefixes[i];
        const char *n = name;
        const char *p = prefix;
        while (*p && *n == *p) { n++; p++; }
        if (*p == '\0') return true;
    }
    return false;
}

PROF_NOINST
static void shm_tree_broadcast(u32 num_frames) {
    if (!state.shm_tree_ptr) return;

    // Build sorted edge list from edge_accum_map
    u32 count = 0;
    shm_edge staging[MAX_BROADCAST_EDGES];

    for (u32 i = 0; i < EDGE_MAP_SIZE && count < MAX_BROADCAST_EDGES; i++) {
        edge_entry *e = &state.edge_accum_map[i];
        if (e->child_addr == nullptr) continue;

        const char *parent_name = e->parent_addr ? resolve_name(e->parent_addr) : "";
        const char *child_name  = resolve_name(e->child_addr);

        i64 avg_total = e->total_ns / (i64)num_frames;
        u32 avg_calls = (e->call_count + num_frames / 2) / num_frames;
        i64 avg_per   = avg_calls > 0 ? avg_total / (i64)avg_calls : 0;

        if (avg_total < 100) continue;

        // Skip edges involving vendor noise functions
        if (profiler_is_vendor_noise(child_name)) continue;

        shm_copy_name(staging[count].parent_name, SHM_EDGE_NAME_LEN, parent_name);
        shm_copy_name(staging[count].child_name, SHM_EDGE_NAME_LEN, child_name);
        staging[count].call_count = avg_calls;
        staging[count]._pad       = 0;
        staging[count].total_ns   = avg_total;
        staging[count].max_ns     = e->max_ns;
        staging[count].avg_ns     = avg_per;
        count++;
    }

    // Sort edges by total_ns descending
    for (u32 i = 1; i < count; i++) {
        shm_edge tmp = staging[i];
        u32 j = i;
        while (j > 0 && staging[j - 1].total_ns < tmp.total_ns) {
            staging[j] = staging[j - 1];
            j--;
        }
        staging[j] = tmp;
    }

    // Write data first, then sequence
    state.shm_tree_ptr->frame_time_ns = state.completed_frame.frame_time_ns;
    state.shm_tree_ptr->edge_count = count;
    memcpy(state.shm_tree_ptr->edges, staging, count * sizeof(shm_edge));

    state.shm_tree_ptr->sequence = state.shm_sequence;
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
            u32 len = cstr_len(z->name);
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

// --- Edge accumulation helpers ---

PROF_NOINST
static void edge_accum_merge_frame(void) {
    for (u32 i = 0; i < EDGE_MAP_SIZE; i++) {
        edge_entry *src = &state.edge_map[i];
        if (src->child_addr == nullptr) continue;

        u32 h = hash_edge(src->parent_addr, src->child_addr);
        for (u32 probe = 0; probe < EDGE_MAP_SIZE; probe++) {
            u32 slot = (h + probe) & (EDGE_MAP_SIZE - 1);
            edge_entry *dst = &state.edge_accum_map[slot];

            if (dst->child_addr == nullptr) {
                *dst = *src;
                break;
            }
            if (dst->parent_addr == src->parent_addr && dst->child_addr == src->child_addr) {
                dst->call_count += src->call_count;
                dst->total_ns  += src->total_ns;
                if (src->max_ns > dst->max_ns) dst->max_ns = src->max_ns;
                break;
            }
        }
    }
}

PROF_NOINST
static void edge_lifetime_merge_frame(void) {
    for (u32 i = 0; i < EDGE_MAP_SIZE; i++) {
        edge_entry *src = &state.edge_map[i];
        if (src->child_addr == nullptr) continue;

        u32 h = hash_edge(src->parent_addr, src->child_addr);
        for (u32 probe = 0; probe < EDGE_MAP_SIZE; probe++) {
            u32 slot = (h + probe) & (EDGE_MAP_SIZE - 1);
            edge_entry *dst = &state.edge_lifetime_map[slot];

            if (dst->child_addr == nullptr) {
                *dst = *src;
                break;
            }
            if (dst->parent_addr == src->parent_addr && dst->child_addr == src->child_addr) {
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
#if defined(PLATFORM_MACOS) || defined(PLATFORM_LINUX)
    state.shm_tree_fd = -1;
#endif
    state.enabled = true;
    state.initialized = true;

#if defined(PLATFORM_WINDOWS)
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
#endif

    shm_create();
    shm_tree_create();
    RL_INFO("Profiler initialized (shared memory broadcast active)");
}

PROF_NOINST
void rl_profiler_shutdown(void) {
    if (!state.initialized) return;
    shm_tree_destroy();
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

    // Flat aggregation (existing)
    u32 hash = hash_ptr(fn);
    for (u32 probe = 0; probe < AGG_MAP_SIZE; probe++) {
        u32 slot = (hash + probe) & (AGG_MAP_SIZE - 1);
        agg_entry *e = &state.agg_map[slot];

        if (e->fn_addr == nullptr) {
            e->fn_addr    = fn;
            e->call_count = 1;
            e->total_ns   = elapsed_ns;
            e->max_ns     = elapsed_ns;
            break;
        }
        if (e->fn_addr == fn) {
            e->call_count++;
            e->total_ns += elapsed_ns;
            if (elapsed_ns > e->max_ns) e->max_ns = elapsed_ns;
            break;
        }
    }

    // Parent-child edge aggregation
    void *parent_addr = state.stack_depth > 0
        ? state.call_stack[state.stack_depth - 1].fn_addr : nullptr;

    u32 ehash = hash_edge(parent_addr, fn);
    for (u32 probe = 0; probe < EDGE_MAP_SIZE; probe++) {
        u32 slot = (ehash + probe) & (EDGE_MAP_SIZE - 1);
        edge_entry *ee = &state.edge_map[slot];

        if (ee->child_addr == nullptr) {
            ee->parent_addr = parent_addr;
            ee->child_addr  = fn;
            ee->call_count  = 1;
            ee->total_ns    = elapsed_ns;
            ee->max_ns      = elapsed_ns;
            return;
        }
        if (ee->parent_addr == parent_addr && ee->child_addr == fn) {
            ee->call_count++;
            ee->total_ns += elapsed_ns;
            if (elapsed_ns > ee->max_ns) ee->max_ns = elapsed_ns;
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
    edge_accum_merge_frame();
    edge_lifetime_merge_frame();
    state.accum_frame_ns += frame_ns;
    state.accum_frames++;
    state.lifetime_total_ns += frame_ns;
    state.lifetime_frames++;

    // Clear per-frame raw maps
    memset(state.agg_map, 0, sizeof(state.agg_map));
    memset(state.edge_map, 0, sizeof(state.edge_map));
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

        // Skip vendor noise (Clay internals, cglm math, NEON intrinsics).
        // These are high-call-count trivial functions where instrumentation
        // overhead dominates. They still appear in session reports.
        if (name && profiler_is_vendor_noise(name)) continue;

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
    shm_tree_broadcast(num_frames);

    // Reset accumulators for next interval
    memset(state.accum_map, 0, sizeof(state.accum_map));
    memset(state.edge_accum_map, 0, sizeof(state.edge_accum_map));
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
            u32 len = cstr_len(zones[i].name);
            if (len >= SHM_ZONE_NAME_LEN) len = SHM_ZONE_NAME_LEN - 1;
            memcpy(hdr.zones[i].name, zones[i].name, len);
        }
        hdr.zones[i].call_count = zones[i].call_count;
        hdr.zones[i].total_ns   = zones[i].total_ns;
        hdr.zones[i].max_ns     = zones[i].max_ns;
        hdr.zones[i].avg_ns     = zones[i].avg_ns;
    }

    fwrite(&hdr, sizeof(hdr), 1, f);

    // Append call tree edge data
    u32 edge_count = 0;
    shm_edge edges[MAX_BROADCAST_EDGES];

    for (u32 i = 0; i < EDGE_MAP_SIZE && edge_count < MAX_BROADCAST_EDGES; i++) {
        edge_entry *e = &state.edge_lifetime_map[i];
        if (e->child_addr == nullptr) continue;

        const char *parent_name = e->parent_addr ? resolve_name(e->parent_addr) : "";
        const char *child_name  = resolve_name(e->child_addr);

        i64 avg_total = e->total_ns / (i64)num_frames;
        u32 avg_calls = (e->call_count + num_frames / 2) / num_frames;
        i64 avg_per   = avg_calls > 0 ? avg_total / (i64)avg_calls : 0;

        if (avg_total < 100) continue;

        shm_copy_name(edges[edge_count].parent_name, SHM_EDGE_NAME_LEN, parent_name);
        shm_copy_name(edges[edge_count].child_name, SHM_EDGE_NAME_LEN, child_name);
        edges[edge_count].call_count = avg_calls;
        edges[edge_count]._pad       = 0;
        edges[edge_count].total_ns   = avg_total;
        edges[edge_count].max_ns     = e->max_ns;
        edges[edge_count].avg_ns     = avg_per;
        edge_count++;
    }

    // Sort edges by total_ns descending
    for (u32 i = 1; i < edge_count; i++) {
        shm_edge tmp = edges[i];
        u32 j = i;
        while (j > 0 && edges[j - 1].total_ns < tmp.total_ns) {
            edges[j] = edges[j - 1];
            j--;
        }
        edges[j] = tmp;
    }

    u32 edge_magic = 0x524C5054; // "RLPT"
    fwrite(&edge_magic, sizeof(u32), 1, f);
    fwrite(&edge_count, sizeof(u32), 1, f);
    if (edge_count > 0) {
        fwrite(edges, sizeof(shm_edge) * edge_count, 1, f);
    }

    fclose(f);
    RL_INFO("Profiler: session report written to %s (%u frames, %u zones, %u edges)",
            path, num_frames, count, edge_count);
}

#endif // RL_PROFILE_ENABLED

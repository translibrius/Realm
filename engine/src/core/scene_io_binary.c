#include "core/scene_io.h"

#include "asset/asset.h"
#include "core/component.h"
#include "core/entity.h"
#include "core/logger.h"
#include "memory/arena.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#include <string.h>

// --- Binary scene format ---
//
// [Header — 16 bytes]
//   magic:        u8[4]  = 'R','L','S','C'
//   version:      u32
//   entity_count: u32
//   string_count: u32
//
// [String table]
//   Per string: { u32 len, u8[len] chars } (no null, no padding)
//
// [Scene name]
//   name_str_idx: u32
//
// [Entities]
//   Per entity:
//     comp_mask: u32  (bit 0=name, 1=transform, 2=mesh, 3=light, 4=behavior)
//     [bit 0] name_str_idx: u32
//     [bit 1] position: f32[3], rotation: f32[3], scale: f32[3]
//     [bit 2] primitive: u32, kind: u32, wireframe: u32,
//             mesh_asset_idx: u32, diffuse_map_idx: u32,
//             specular: f32[3], shininess: f32
//     [bit 3] ambient: f32[3], diffuse: f32[3], specular: f32[3]
//     [bit 4] behavior_str_idx: u32

#define RLSC_MAGIC_0 'R'
#define RLSC_MAGIC_1 'L'
#define RLSC_MAGIC_2 'S'
#define RLSC_MAGIC_3 'C'
#define RLSC_VERSION 1

#define RLSC_STR_NONE UINT32_MAX

#define COMP_NAME      (1u << 0)
#define COMP_TRANSFORM (1u << 1)
#define COMP_MESH      (1u << 2)
#define COMP_LIGHT     (1u << 3)
#define COMP_BEHAVIOR  (1u << 4)

// --- String table (write side) ---

typedef struct string_table {
    const char **strings; // pointer array (arena-allocated)
    u32         *lengths; // length array  (arena-allocated)
    u32          count;
    u32          capacity;
    rl_arena    *arena;
} string_table;

static void strtab_init(string_table *st, rl_arena *arena, u32 capacity) {
    st->arena    = arena;
    st->capacity = capacity;
    st->count    = 0;
    st->strings  = rl_arena_push(arena, capacity * sizeof(const char *), true);
    st->lengths  = rl_arena_push(arena, capacity * sizeof(u32), true);
}

// Returns index of string in table. Deduplicates by pointer content.
static u32 strtab_add(string_table *st, const char *str) {
    if (!str) return RLSC_STR_NONE;

    u32 len = cstr_len(str);

    // Linear scan for dedup — fine for scene-sized data
    for (u32 i = 0; i < st->count; i++) {
        if (st->lengths[i] == len && memcmp(st->strings[i], str, len) == 0) {
            return i;
        }
    }

    // Grow if needed (shouldn't happen with reasonable initial capacity)
    if (st->count >= st->capacity) {
        u32 new_cap = st->capacity * 2;
        const char **new_strings = rl_arena_push(st->arena, new_cap * sizeof(const char *), true);
        u32 *new_lengths = rl_arena_push(st->arena, new_cap * sizeof(u32), true);
        memcpy(new_strings, st->strings, st->count * sizeof(const char *));
        memcpy(new_lengths, st->lengths, st->count * sizeof(u32));
        st->strings  = new_strings;
        st->lengths  = new_lengths;
        st->capacity = new_cap;
    }

    u32 idx = st->count++;
    st->strings[idx] = str;
    st->lengths[idx] = len;
    return idx;
}

// --- Write buffer (arena-backed, grows by doubling) ---

typedef struct write_buf {
    u8       *data;
    u64       size;
    u64       capacity;
    rl_arena *arena;
} write_buf;

static void wbuf_init(write_buf *wb, rl_arena *arena, u64 initial_cap) {
    wb->arena    = arena;
    wb->capacity = initial_cap;
    wb->size     = 0;
    wb->data     = rl_arena_push(arena, initial_cap, false);
}

static void wbuf_ensure(write_buf *wb, u64 additional) {
    if (wb->size + additional <= wb->capacity) return;
    u64 new_cap = wb->capacity * 2;
    while (new_cap < wb->size + additional) new_cap *= 2;
    u8 *new_data = rl_arena_push(wb->arena, new_cap, false);
    memcpy(new_data, wb->data, wb->size);
    wb->data     = new_data;
    wb->capacity = new_cap;
}

static void wbuf_write(write_buf *wb, const void *src, u64 len) {
    wbuf_ensure(wb, len);
    memcpy(wb->data + wb->size, src, len);
    wb->size += len;
}

static void wbuf_write_u8(write_buf *wb, u8 v)   { wbuf_write(wb, &v, 1); }
static void wbuf_write_u32(write_buf *wb, u32 v)  { wbuf_write(wb, &v, 4); }
static void wbuf_write_f32(write_buf *wb, f32 v)  { wbuf_write(wb, &v, 4); }

static void wbuf_write_vec3(write_buf *wb, const f32 *v) {
    wbuf_write_f32(wb, v[0]);
    wbuf_write_f32(wb, v[1]);
    wbuf_write_f32(wb, v[2]);
}

// --- Read cursor ---

typedef struct read_cursor {
    const u8 *data;
    u64       size;
    u64       pos;
} read_cursor;

static b8 rc_can_read(const read_cursor *rc, u64 n) {
    return rc->pos + n <= rc->size;
}

static b8 rc_read(read_cursor *rc, void *dst, u64 n) {
    if (!rc_can_read(rc, n)) return false;
    memcpy(dst, rc->data + rc->pos, n);
    rc->pos += n;
    return true;
}

static b8 rc_read_u8(read_cursor *rc, u8 *out)   { return rc_read(rc, out, 1); }
static b8 rc_read_u32(read_cursor *rc, u32 *out) { return rc_read(rc, out, 4); }
static b8 rc_read_f32(read_cursor *rc, f32 *out) { return rc_read(rc, out, 4); }

static b8 rc_read_vec3(read_cursor *rc, f32 *out) {
    return rc_read_f32(rc, &out[0])
        && rc_read_f32(rc, &out[1])
        && rc_read_f32(rc, &out[2]);
}

// --- Public API ---

b8 scene_save_binary(const rl_scene *scene, const char *path) {
    if (!scene || !path) return false;

    ARENA_SCRATCH_START();

    // Build string table
    string_table st;
    strtab_init(&st, scratch.arena, 256);

    const rl_entity_store *es    = &scene->entities;
    const rl_component_store *cs = &scene->components;

    u32 scene_name_idx = strtab_add(&st, scene->name);

    // Count alive entities and collect strings
    u32 entity_count = 0;
    for (u32 i = 1; i < es->high_water; i++) {
        if (!es->alive[i]) continue;
        entity_count++;

        if (cs->has_name[i])     strtab_add(&st, cs->names[i].name);
        if (cs->has_behavior[i]) strtab_add(&st, cs->behaviors[i].name);

        if (cs->has_mesh[i]) {
            if (cs->meshes[i].mesh_asset) {
                rl_asset *a = asset_get(cs->meshes[i].mesh_asset);
                if (a && a->source_path) strtab_add(&st, a->source_path);
            }
            if (cs->meshes[i].material.diffuse_map) {
                rl_asset *a = asset_get(cs->meshes[i].material.diffuse_map);
                if (a && a->source_path) strtab_add(&st, a->source_path);
            }
        }
    }

    // Write to buffer
    write_buf wb;
    wbuf_init(&wb, scratch.arena, 4096);

    // Header
    wbuf_write_u8(&wb, RLSC_MAGIC_0);
    wbuf_write_u8(&wb, RLSC_MAGIC_1);
    wbuf_write_u8(&wb, RLSC_MAGIC_2);
    wbuf_write_u8(&wb, RLSC_MAGIC_3);
    wbuf_write_u32(&wb, RLSC_VERSION);
    wbuf_write_u32(&wb, entity_count);
    wbuf_write_u32(&wb, st.count);

    // String table
    for (u32 i = 0; i < st.count; i++) {
        wbuf_write_u32(&wb, st.lengths[i]);
        wbuf_write(&wb, st.strings[i], st.lengths[i]);
    }

    // Scene name
    wbuf_write_u32(&wb, scene_name_idx);

    // Entities
    for (u32 i = 1; i < es->high_water; i++) {
        if (!es->alive[i]) continue;

        u32 mask = 0;
        if (cs->has_name[i])      mask |= COMP_NAME;
        if (cs->has_transform[i]) mask |= COMP_TRANSFORM;
        if (cs->has_mesh[i])      mask |= COMP_MESH;
        if (cs->has_light[i])     mask |= COMP_LIGHT;
        if (cs->has_behavior[i])  mask |= COMP_BEHAVIOR;
        wbuf_write_u32(&wb, mask);

        // Name
        if (mask & COMP_NAME) {
            wbuf_write_u32(&wb, strtab_add(&st, cs->names[i].name));
        }

        // Transform
        if (mask & COMP_TRANSFORM) {
            const rl_transform *t = &cs->transforms[i];
            wbuf_write_vec3(&wb, t->position);
            wbuf_write_vec3(&wb, t->rotation);
            wbuf_write_vec3(&wb, t->scale);
        }

        // Mesh
        if (mask & COMP_MESH) {
            const rl_mesh_component *m = &cs->meshes[i];
            wbuf_write_u32(&wb, (u32)m->primitive);
            wbuf_write_u32(&wb, (u32)m->kind);
            wbuf_write_u32(&wb, m->wireframe ? 1 : 0);

            // Mesh asset path
            u32 mesh_idx = RLSC_STR_NONE;
            if (m->mesh_asset) {
                rl_asset *a = asset_get(m->mesh_asset);
                if (a && a->source_path) mesh_idx = strtab_add(&st, a->source_path);
            }
            wbuf_write_u32(&wb, mesh_idx);

            // Diffuse map path
            u32 diff_idx = RLSC_STR_NONE;
            if (m->material.diffuse_map) {
                rl_asset *a = asset_get(m->material.diffuse_map);
                if (a && a->source_path) diff_idx = strtab_add(&st, a->source_path);
            }
            wbuf_write_u32(&wb, diff_idx);

            wbuf_write_vec3(&wb, m->material.specular);
            wbuf_write_f32(&wb, m->material.shininess);
        }

        // Light
        if (mask & COMP_LIGHT) {
            const rl_light_component *l = &cs->lights[i];
            wbuf_write_vec3(&wb, l->ambient);
            wbuf_write_vec3(&wb, l->diffuse);
            wbuf_write_vec3(&wb, l->specular);
        }

        // Behavior
        if (mask & COMP_BEHAVIOR) {
            wbuf_write_u32(&wb, strtab_add(&st, cs->behaviors[i].name));
        }
    }

    b8 ok = platform_file_write_all(path, wb.data, wb.size);
    if (!ok) {
        RL_ERROR("scene_save_binary: failed to write '%s'", path);
    }

    ARENA_SCRATCH_RELEASE();
    return ok;
}

rl_scene *scene_load_binary(const char *path) {
    if (!path) return nullptr;

    // Read entire file
    rl_file file = {0};
    if (!platform_file_open(path, P_FILE_READ, &file)) {
        RL_ERROR("scene_load_binary: failed to open '%s'", path);
        return nullptr;
    }
    if (!platform_file_read_all(&file)) {
        RL_ERROR("scene_load_binary: failed to read '%s'", path);
        platform_file_close(&file);
        return nullptr;
    }

    read_cursor rc = { .data = file.buf, .size = file.buf_len, .pos = 0 };

    // Header
    u8 m0, m1, m2, m3;
    u32 version, entity_count, string_count;

    if (!rc_read_u8(&rc, &m0) || !rc_read_u8(&rc, &m1) ||
        !rc_read_u8(&rc, &m2) || !rc_read_u8(&rc, &m3)) {
        RL_ERROR("scene_load_binary: '%s' truncated header", path);
        platform_file_close(&file);
        return nullptr;
    }

    if (m0 != RLSC_MAGIC_0 || m1 != RLSC_MAGIC_1 ||
        m2 != RLSC_MAGIC_2 || m3 != RLSC_MAGIC_3) {
        RL_ERROR("scene_load_binary: '%s' invalid magic bytes", path);
        platform_file_close(&file);
        return nullptr;
    }

    if (!rc_read_u32(&rc, &version) || !rc_read_u32(&rc, &entity_count) ||
        !rc_read_u32(&rc, &string_count)) {
        RL_ERROR("scene_load_binary: '%s' truncated header", path);
        platform_file_close(&file);
        return nullptr;
    }

    if (version != RLSC_VERSION) {
        RL_ERROR("scene_load_binary: '%s' unsupported version %u (expected %u)",
                 path, version, RLSC_VERSION);
        platform_file_close(&file);
        return nullptr;
    }

    // Read string table into temp buffers on scratch arena
    ARENA_SCRATCH_START();

    // Allocate string pointer + length arrays
    char **str_ptrs = rl_arena_push(scratch.arena, string_count * sizeof(char *), true);
    u32   *str_lens = rl_arena_push(scratch.arena, string_count * sizeof(u32), true);

    for (u32 i = 0; i < string_count; i++) {
        u32 len;
        if (!rc_read_u32(&rc, &len)) {
            RL_ERROR("scene_load_binary: '%s' truncated string table", path);
            ARENA_SCRATCH_RELEASE();
            platform_file_close(&file);
            return nullptr;
        }
        if (!rc_can_read(&rc, len)) {
            RL_ERROR("scene_load_binary: '%s' truncated string data", path);
            ARENA_SCRATCH_RELEASE();
            platform_file_close(&file);
            return nullptr;
        }
        // Copy to null-terminated buffer on scratch arena
        char *buf = rl_arena_push(scratch.arena, len + 1, false);
        memcpy(buf, rc.data + rc.pos, len);
        buf[len] = '\0';
        rc.pos += len;

        str_ptrs[i] = buf;
        str_lens[i] = len;
    }

    // Helper to resolve string index
    #define STR_AT(idx) ((idx) < string_count ? str_ptrs[(idx)] : nullptr)

    // Scene name
    u32 scene_name_idx;
    if (!rc_read_u32(&rc, &scene_name_idx)) {
        RL_ERROR("scene_load_binary: '%s' truncated scene name", path);
        ARENA_SCRATCH_RELEASE();
        platform_file_close(&file);
        return nullptr;
    }

    const char *scene_name = STR_AT(scene_name_idx);
    rl_scene *scene = scene_create(scene_name ? scene_name : "Untitled");
    if (!scene) {
        ARENA_SCRATCH_RELEASE();
        platform_file_close(&file);
        return nullptr;
    }

    // Entities
    for (u32 ei = 0; ei < entity_count; ei++) {
        u32 mask;
        if (!rc_read_u32(&rc, &mask)) {
            RL_ERROR("scene_load_binary: '%s' truncated entity %u", path, ei);
            scene_destroy(scene);
            ARENA_SCRATCH_RELEASE();
            platform_file_close(&file);
            return nullptr;
        }

        // Name
        const char *ent_name = "Entity";
        if (mask & COMP_NAME) {
            u32 name_idx;
            if (!rc_read_u32(&rc, &name_idx)) goto truncated;
            const char *s = STR_AT(name_idx);
            if (s) ent_name = s;
        }

        rl_entity e = scene_entity_create(scene, ent_name);

        // Transform
        if (mask & COMP_TRANSFORM) {
            rl_transform *t = transform_add(&scene->components, e);
            if (!rc_read_vec3(&rc, t->position) ||
                !rc_read_vec3(&rc, t->rotation) ||
                !rc_read_vec3(&rc, t->scale)) goto truncated;
            t->dirty = true;
        }

        // Mesh
        if (mask & COMP_MESH) {
            rl_mesh_component *m = mesh_add(&scene->components, e);
            u32 prim, kind, wire, mesh_asset_idx, diffuse_idx;

            if (!rc_read_u32(&rc, &prim) || !rc_read_u32(&rc, &kind) ||
                !rc_read_u32(&rc, &wire) || !rc_read_u32(&rc, &mesh_asset_idx) ||
                !rc_read_u32(&rc, &diffuse_idx) ||
                !rc_read_vec3(&rc, m->material.specular) ||
                !rc_read_f32(&rc, &m->material.shininess)) goto truncated;

            m->primitive = (rl_frame_primitive)prim;
            m->kind      = (rl_frame_mesh_kind)kind;
            m->wireframe = wire != 0;

            const char *mesh_path = (mesh_asset_idx != RLSC_STR_NONE) ? STR_AT(mesh_asset_idx) : nullptr;
            m->mesh_asset = mesh_path ? asset_find(mesh_path) : 0;

            const char *diff_path = (diffuse_idx != RLSC_STR_NONE) ? STR_AT(diffuse_idx) : nullptr;
            m->material.diffuse_map = diff_path ? asset_find(diff_path) : 0;
        }

        // Light
        if (mask & COMP_LIGHT) {
            rl_light_component *l = light_add(&scene->components, e);
            if (!rc_read_vec3(&rc, l->ambient) ||
                !rc_read_vec3(&rc, l->diffuse) ||
                !rc_read_vec3(&rc, l->specular)) goto truncated;
        }

        // Behavior
        if (mask & COMP_BEHAVIOR) {
            u32 beh_idx;
            if (!rc_read_u32(&rc, &beh_idx)) goto truncated;
            const char *beh_name = STR_AT(beh_idx);
            if (beh_name) behavior_comp_add(&scene->components, e, beh_name);
        }

        continue;
    truncated:
        RL_ERROR("scene_load_binary: '%s' truncated at entity %u", path, ei);
        scene_destroy(scene);
        ARENA_SCRATCH_RELEASE();
        platform_file_close(&file);
        return nullptr;
    }

    #undef STR_AT

    ARENA_SCRATCH_RELEASE();
    platform_file_close(&file);
    RL_INFO("Scene '%s' loaded (binary) from '%s'", scene->name, path);
    return scene;
}

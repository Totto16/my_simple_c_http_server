/*
 * zmap.h
 *
 * This file is part of the z-libs collection: https://github.com/z-libs
 * Licensed under the MIT License.
 * 
 * based on https://github.com/z-libs/zmap.h/blob/743b276cfccf65b8f4a07aa1175c12fc5359b525/zmap.h
 * 
 * modified to suit my needs
 * 
 * Modifications by: Totto16
 */

#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>


// Return Codes.
typedef enum : bool {
    ZmapResultErr = false,
    ZmapResultOk = true,
} ZmapResult;

#define ZMAP_WOULD_OVERWRITE ((void*)(1))
#define ZMAP_NO_ELEMENT_HERE ((void*)(2))

typedef enum : uint8_t {
    ZmapInsertResultOk = 0,
    ZmapInsertResultErr = 1,
    ZmapInsertResultWouldOverwrite = 2
} ZmapInsertResult;

// Memory Macros.
// If the user hasn't defined their own allocator, use the standard one.
#ifndef Z_MALLOC
    #define Z_MALLOC(sz)       malloc(sz)
    #define Z_CALLOC(n, sz)    calloc(n, sz)
    #define Z_REALLOC(p, sz)   realloc(p, sz)
    #define Z_FREE(p)          free(p)
#endif

// Compiler Extensions (Optional).
// We check for GCC/Clang features to enable RAII-style cleanup.
// Define Z_NO_EXTENSIONS to disable this manually.
#if !defined(Z_NO_EXTENSIONS) && (defined(__GNUC__) || defined(__clang__))
    #define Z_HAS_CLEANUP 1
    #define Z_CLEANUP(func) __attribute__((cleanup(func)))
#else
    #define Z_HAS_CLEANUP 0
    #define Z_CLEANUP(func) 
#endif


#ifndef Z_MAP_MALLOC
    #define Z_MAP_MALLOC(sz)      Z_MALLOC(sz)
#endif
#ifndef Z_MAP_CALLOC
    #define Z_MAP_CALLOC(n, sz)   Z_CALLOC(n, sz)
#endif
#ifndef Z_MAP_FREE
    #define Z_MAP_FREE(p)         Z_FREE(p)
#endif


#if __STDC_VERSION__ >= 202311L || defined(__cplusplus)
#define STATIC_ASSERT(check, message) static_assert(check, message)
#elif __STDC_VERSION__ < 201112L
// empty, as not supported
#define STATIC_ASSERT(check, message)
#else
#define STATIC_ASSERT(check, message) _Static_assert(check, message)
#endif


// maybe some visibility things later, but I just removed the ZMAP_FUN_ATTRIBUTES
#define ZMAP_FUN_ATTRIBUTES 

typedef size_t ZmapHashType;

ZMAP_FUN_ATTRIBUTES ZmapHashType zmap_default_hash(const void *key, size_t len);

ZMAP_FUN_ATTRIBUTES ZmapHashType zmap_stbds_hash_string(const char* str);

ZMAP_FUN_ATTRIBUTES ZmapHashType zmap_stbds_hash_bytes(const void *key, size_t len);


typedef enum 
{
    ZMAP_EMPTY = 0,
    ZMAP_OCCUPIED,
    ZMAP_DELETED
} zmap_state;

#define ZMAP_HASH_SCALAR(k)          zmap_default_hash(&(k), sizeof(k))
#define ZMAP_HASH_STR(k)             zmap_stbds_hash_string(k)
#define ZMAP_HASH_BYTES(start, len)  zmap_stbds_hash_bytes((start), len)


#define ZMAP_HASH_FUNC_NAME(KeyName) zmap_hash_type_impl_##KeyName

#define ZMAP_COMPARE_FUNC_NAME(KeyName) zmap_compare_type_impl_##KeyName

#define ZMAP_HASH_FUNC_SIG(KeyT, KeyName) ZMAP_FUN_ATTRIBUTES ZmapHashType ZMAP_HASH_FUNC_NAME(KeyName)(const KeyT key)

#define ZMAP_COMPARE_FUNC_SIG(KeyT, KeyName) ZMAP_FUN_ATTRIBUTES int ZMAP_COMPARE_FUNC_NAME(KeyName)(const KeyT key1, const KeyT key2)

#define ZMAP_TYPENAME_ENTRY(TypeName) zmap_entry_##TypeName

#define ZMAP_TYPENAME_BUCKET(TypeName) zmap_bucket_##TypeName

#define ZMAP_TYPENAME_MAP(TypeName) zmap_##TypeName

#define ZMAP_DEFINE_MAP_TYPE(KeyT, KeyName, ValT, Name)                                                                       \
                                                                                                                \
typedef struct {                                                                                                \
    KeyT key;                                                                                                   \
    ValT value;                                                                                                 \
} ZMAP_TYPENAME_ENTRY(Name);                                                                                    \
                                                                                                                \
typedef struct {                                                                                                \
    ZMAP_TYPENAME_ENTRY(Name) entry;                                                                                                   \
    zmap_state state;                                                                                          \
} ZMAP_TYPENAME_BUCKET(Name);                                                                                           \
                                                                                                                \
typedef struct {                                                                                                \
    ZMAP_TYPENAME_BUCKET(Name) *buckets;                                                                                \
    size_t capacity;                                                                                            \
    size_t count;                                                                                               \
    size_t occupied;                                                                                            \
} ZMAP_TYPENAME_MAP(Name);                                                                                                   \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES void zmap_free_##Name(ZMAP_TYPENAME_MAP(Name) *m);                                          \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES [[nodiscard]] ZMAP_TYPENAME_MAP(Name) zmap_init_##Name(void);        \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES [[nodiscard]] ZmapResult zmap_resize_##Name(ZMAP_TYPENAME_MAP(Name) *m, size_t new_cap);                   \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES [[nodiscard]] ValT* zmap_insert_slot_##Name(ZMAP_TYPENAME_MAP(Name) *m, KeyT key, bool allow_overwrite);       \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES [[nodiscard]] ZmapInsertResult zmap_insert_##Name(ZMAP_TYPENAME_MAP(Name) *m, KeyT key, const ValT val, bool allow_overwrite);                 \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES [[nodiscard]] inline ValT* zmap_put_slot_##Name(ZMAP_TYPENAME_MAP(Name) *m, KeyT key){       \
    return zmap_insert_slot_##Name(m, key, true);                                                               \
}                                                                                                               \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES [[nodiscard]] inline ZmapResult zmap_put_##Name(ZMAP_TYPENAME_MAP(Name) *m, KeyT key, const ValT val){                 \
    const ZmapInsertResult result = zmap_insert_##Name(m, key, val, true);                                                                \
    return result == ZmapInsertResultOk ? ZmapResultOk : ZmapResultErr;                                         \
}                                                                                                               \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES [[nodiscard]] ValT* zmap_get_mut_##Name(ZMAP_TYPENAME_MAP(Name) *m, const KeyT key);             \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES [[nodiscard]] const ValT* zmap_get_##Name(const ZMAP_TYPENAME_MAP(Name) *m, const KeyT key);             \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES [[nodiscard]] const ZMAP_TYPENAME_ENTRY(Name)* zmap_get_entry_at_##Name(const ZMAP_TYPENAME_MAP(Name) *m, size_t index);             \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES void zmap_remove_##Name(ZMAP_TYPENAME_MAP(Name) *m, const KeyT key);                        \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES void zmap_clear_##Name(ZMAP_TYPENAME_MAP(Name) *m);                                         \
                                                                                                                \
ZMAP_HASH_FUNC_SIG(KeyT, KeyName);                                                                              \
                                                                                                                \
ZMAP_COMPARE_FUNC_SIG(KeyT, KeyName);


#define ZMAP_ASSERT_SHOULD_USE_INSERT(val) STATIC_ASSERT(sizeof(val) <= 8, "only small values should use insert, use insert slot for larger ones instead!")

#define ZMAP_ASSERT_SHOULD_USE_INSERT_SLOT(val) STATIC_ASSERT(sizeof(val) > 8, "only big values should use insert slot, use insert for smaller ones instead!")


#define ZMAP_PUT(Name, Map, Key, Value) zmap_put_##Name(Map, Key, Value)
#define ZMAP_PUT_SLOT(Name, Map, Key) zmap_put_slot_##Name(Map, Key)
#define ZMAP_INSERT(Name, Map, Key, Value, AllowOverwrite) zmap_insert_##Name(Map, Key, Value, AllowOverwrite)
#define ZMAP_INSERT_SLOT(Name, Map, Key, AllowOverwrite) zmap_insert_slot_##Name(Map, Key, AllowOverwrite)
#define ZMAP_GET(Name, Map, Key)                        zmap_get_##Name(Map, Key)
#define ZMAP_GET_MUT(Name, Map, Key)                    zmap_get_mut_##Name(Map, Key)
#define ZMAP_GET_ENTRY_AT(Name, Map, Index)             zmap_get_entry_at_##Name(Map, Index)
#define ZMAP_REM(Name, Map, Key)        zmap_remove_##Name(Map, Key)
#define ZMAP_FREE(Name, Map)            zmap_free_##Name(Map)
#define ZMAP_CLEAR(Name, Map)           zmap_clear_##Name(Map)

#define ZMAP_INIT(Name) zmap_init_##Name()

#define ZMAP_SIZE(v) (v).count
#define ZMAP_IS_EMPTY(v) ((v).count == 0)
#define ZMAP_CAPACITY(v) (v).capacity
#define ZMAP_OCCUPIED_COUNT(v) (v).occupied

#if defined(Z_HAS_CLEANUP) && Z_HAS_CLEANUP
    #define zmap_autofree(Name)  Z_CLEANUP(map_free_##Name) ZMAP_TYPENAME_MAP(Name)
#endif

#if !defined(Z_NO_GENERIC_USAGE)

#define M_PUT_ENTRY(K, V, N)    zmap_##N*: zmap_put_##N,
#define M_GET_ENTRY(K, V, N)    zmap_##N*: zmap_get_##N,
#define M_REM_ENTRY(K, V, N)    zmap_##N*: zmap_remove_##N,
#define M_FREE_ENTRY(K, V, N)   zmap_##N*: zmap_free_##N,
#define M_CLEAR_ENTRY(K, V, N)  zmap_##N*: zmap_clear_##N,

#define zmap_put(m, k, v)   _Generic((m), REGISTER_MAP_TYPES(M_PUT_ENTRY)  default: 0) (m, k, v)
#define zmap_get(m, k)      _Generic((m), REGISTER_MAP_TYPES(M_GET_ENTRY)  default: (void*)0) (m, k)
#define zmap_remove(m, k)   _Generic((m), REGISTER_MAP_TYPES(M_REM_ENTRY)  default: (void)0) (m, k)
#define zmap_free(m)        _Generic((m), REGISTER_MAP_TYPES(M_FREE_ENTRY) default: (void)0) (m)
#define zmap_clear(m)       _Generic((m), REGISTER_MAP_TYPES(M_CLEAR_ENTRY) default: (void)0) (m)


#endif

#define ZMAP_IMPLEMENT_MAP_TYPE(KeyT, KeyName, ValT, Name)                                                                       \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES void zmap_free_##Name(ZMAP_TYPENAME_MAP(Name) *m) {                                                             \
    Z_MAP_FREE(m->buckets);                                                                                     \
    *m = (ZMAP_TYPENAME_MAP(Name)){0};                                                                                       \
}                                                                                                               \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES ZMAP_TYPENAME_MAP(Name) zmap_init_##Name(void) {                           \
    return (ZMAP_TYPENAME_MAP(Name)){ .buckets=NULL, .capacity = 0, .count = 0, .occupied = 0};                 \
}                                                                                                               \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES ZmapResult zmap_resize_##Name(ZMAP_TYPENAME_MAP(Name) *m, size_t new_cap) {                                            \
    if(new_cap < m->occupied){ return ZmapResultErr; }                                                           \
    ZMAP_TYPENAME_BUCKET(Name) *new_buckets = Z_MAP_CALLOC(new_cap, sizeof(ZMAP_TYPENAME_BUCKET(Name)));                        \
    if (!new_buckets) { return ZmapResultErr; };                                                                             \
                                                                                                                \
    for (size_t i = 0; i < m->capacity; i++) {                                                                  \
            if (m->buckets[i].state == ZMAP_OCCUPIED) {                                                         \
                ZmapHashType hash = ZMAP_HASH_FUNC_NAME(KeyName)(m->buckets[i].entry.key);                                                \
                size_t idx = hash % new_cap;                                                                    \
                while (new_buckets[idx].state == ZMAP_OCCUPIED) {                                               \
                    idx = (idx + 1) % new_cap;                                                                  \
                }                                                                                               \
                new_buckets[idx] = m->buckets[i];                                                               \
            }                                                                                                   \
        }                                                                                                       \
        Z_MAP_FREE(m->buckets);                                                                                 \
        m->buckets = new_buckets;                                                                               \
        m->capacity = new_cap;                                                                                  \
        m->occupied = m->count;                                                                                 \
        return ZmapResultOk;                                                                                            \
}                                                                                                               \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES ValT* zmap_insert_slot_##Name(ZMAP_TYPENAME_MAP(Name) *m, KeyT key, bool allow_overwrite) {                                           \
        if (m->occupied >= m->capacity * 0.75) {                                                                \
            size_t new_cap = m->capacity == 0 ? 16 : m->capacity * 2;                                           \
            if (zmap_resize_##Name(m, new_cap) != ZmapResultOk) { return NULL; }                                            \
        }                                                                                                       \
        ZmapHashType hash = ZMAP_HASH_FUNC_NAME(KeyName)(key);                                                                      \
        size_t idx = hash % m->capacity;                                                                        \
        size_t deleted_idx = SIZE_MAX;                                                                          \
                                                                                                                \
        for (size_t i = 0; i < m->capacity; i++) {                                                              \
            zmap_state s = m->buckets[idx].state;                                                               \
            if (s == ZMAP_EMPTY) {                                                                              \
                if (deleted_idx != SIZE_MAX) idx = deleted_idx;                                                 \
                else m->occupied++;                                                                             \
                m->buckets[idx] = (ZMAP_TYPENAME_BUCKET(Name)){ .entry = (ZMAP_TYPENAME_ENTRY(Name)){.key = key, .value = (ValT){}}, .state = ZMAP_OCCUPIED };     \
                m->count++;                                                                                     \
                return &(m->buckets[idx].entry.value);                                                                                    \
            }                                                                                                   \
            if (s == ZMAP_DELETED) {                                                                            \
                if (deleted_idx == SIZE_MAX) deleted_idx = idx;                                                 \
            }                                                                                                   \
            else if (ZMAP_COMPARE_FUNC_NAME(KeyName)(m->buckets[idx].entry.key, key) == 0) {                                              \
                if(!allow_overwrite){ return ZMAP_WOULD_OVERWRITE; }                                                            \
                return &(m->buckets[idx].entry.value);                                                                    \
            }                                                                                                   \
            idx = (idx + 1) % m->capacity;                                                                      \
        }                                                                                                       \
        return NULL;                                                                                           \
    }                                                                                                           \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES ZmapInsertResult zmap_insert_##Name(ZMAP_TYPENAME_MAP(Name) *m, KeyT key, const ValT val, bool allow_overwrite) {                                           \
    ValT* slot = zmap_insert_slot_##Name(m, key, allow_overwrite);                                               \
    if (slot == NULL) { return ZmapInsertResultErr; }                                                              \
    if(slot == ZMAP_WOULD_OVERWRITE) { return ZmapInsertResultWouldOverwrite; }                  \
    *slot = val;                                                                          \
    return ZmapInsertResultOk;                                                                    \
}                                                                                                           \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES ValT* zmap_get_mut_##Name(ZMAP_TYPENAME_MAP(Name) *m, const KeyT key) {                                               \
        if (m->count == 0) return NULL;                                                                         \
        ZmapHashType hash = ZMAP_HASH_FUNC_NAME(KeyName)(key);                                                                      \
        size_t idx = hash % m->capacity;                                                                        \
                                                                                                                \
        for (size_t i = 0; i < m->capacity; i++) {                                                              \
            zmap_state s = m->buckets[idx].state;                                                               \
            if (s == ZMAP_EMPTY) return NULL;                                                                   \
            if (s == ZMAP_OCCUPIED && ZMAP_COMPARE_FUNC_NAME(KeyName)(m->buckets[idx].entry.key, key) == 0) {                             \
                return &(m->buckets[idx].entry.value);                                                                  \
            }                                                                                                   \
            idx = (idx + 1) % m->capacity;                                                                      \
        }                                                                                                       \
        return NULL;                                                                                            \
    }                                                                                                           \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES const ValT* zmap_get_##Name(const ZMAP_TYPENAME_MAP(Name) * const m, const KeyT key) {                                               \
        if (m->count == 0) return NULL;                                                                         \
        ZmapHashType hash = ZMAP_HASH_FUNC_NAME(KeyName)(key);                                                                      \
        size_t idx = hash % m->capacity;                                                                        \
                                                                                                                \
        for (size_t i = 0; i < m->capacity; i++) {                                                              \
            zmap_state s = m->buckets[idx].state;                                                               \
            if (s == ZMAP_EMPTY) return NULL;                                                                   \
            if (s == ZMAP_OCCUPIED && ZMAP_COMPARE_FUNC_NAME(KeyName)(m->buckets[idx].entry.key, key) == 0) {                             \
                return &(m->buckets[idx].entry.value);                                                                  \
            }                                                                                                   \
            idx = (idx + 1) % m->capacity;                                                                      \
        }                                                                                                       \
        return NULL;                                                                                            \
    }                                                                                                           \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES [[nodiscard]] const ZMAP_TYPENAME_ENTRY(Name)* zmap_get_entry_at_##Name(const ZMAP_TYPENAME_MAP(Name) * const m, size_t index){                                                                                                         \
    if(index >= m->capacity){ return NULL; }                                                                    \
                                                                                                                \
    ZMAP_TYPENAME_BUCKET(Name)* hm_bucket = &(m->buckets[index]);                                               \
                                                                                                                \
    if(hm_bucket->state == ZMAP_OCCUPIED) {                                                                     \
        return &(hm_bucket->entry);                                                                              \
    }                                                                                                           \
                                                                                                                \
    return ZMAP_NO_ELEMENT_HERE;                                                                                \
}                                                                                                               \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES void zmap_remove_##Name(ZMAP_TYPENAME_MAP(Name) *m, const KeyT key) {                                             \
        if (m->count == 0) return;                                                                              \
        ZmapHashType hash = ZMAP_HASH_FUNC_NAME(KeyName)(key);                                                                      \
        size_t idx = hash % m->capacity;                                                                        \
                                                                                                                \
        for (size_t i = 0; i < m->capacity; i++) {                                                              \
            zmap_state s = m->buckets[idx].state;                                                               \
            if (s == ZMAP_EMPTY) return;                                                                        \
            if (s == ZMAP_OCCUPIED && ZMAP_COMPARE_FUNC_NAME(KeyName)(m->buckets[idx].entry.key, key) == 0) {                             \
                m->buckets[idx].state = ZMAP_DELETED;                                                           \
                m->count--;                                                                                     \
                return;                                                                                         \
            }                                                                                                   \
            idx = (idx + 1) % m->capacity;                                                                      \
        }                                                                                                       \
    }                                                                                                           \
                                                                                                                \
ZMAP_FUN_ATTRIBUTES void zmap_clear_##Name(ZMAP_TYPENAME_MAP(Name) *m) {                                                        \
        if (m->capacity > 0) {                                                                                  \
             memset(m->buckets, 0, m->capacity * sizeof(ZMAP_TYPENAME_BUCKET(Name)));                                   \
        }                                                                                                       \
        m->count = 0;                                                                                           \
        m->occupied = 0;                                                                                        \
    }

#define ZMAP_DEFINE_AND_IMPLEMENT_MAP_TYPE(KeyT, KeyName, ValT, Name) \
    ZMAP_DEFINE_MAP_TYPE(KeyT, KeyName, ValT, Name)                   \
    ZMAP_IMPLEMENT_MAP_TYPE(KeyT, KeyName, ValT, Name)

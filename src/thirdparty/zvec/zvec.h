/*
 * zvec.h
 * based on https://github.com/z-libs/zvec.h/blob/5ebd8ac1d1ee66e14210037f6938575901f9d17e/zvec.h
 *
 * modified to suit my needs
 *
 * By: Zuhaitz-dev
 * 
 * Modifications by: Totto16
*/

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef enum : bool {
    ZvecResultErr = false,
    ZvecResultOk = true,
} ZvecResult;

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


#ifndef Z_VEC_MALLOC
    #define Z_VEC_MALLOC(sz)      Z_MALLOC(sz)
#endif

#ifndef Z_VEC_CALLOC
    #define Z_VEC_CALLOC(n, sz)   Z_CALLOC(n, sz)
#endif

#ifndef Z_VEC_REALLOC
    #define Z_VEC_REALLOC(p, sz)  Z_REALLOC(p, sz)
#endif

#ifndef Z_VEC_FREE
    #define Z_VEC_FREE(p)         Z_FREE(p)
#endif


#if __STDC_VERSION__ >= 202311L || defined(__cplusplus)
#define STATIC_ASSERT(check, message) static_assert(check, message)
#elif __STDC_VERSION__ < 201112L
// empty, as not supported
#define STATIC_ASSERT(check, message)
#else
#define STATIC_ASSERT(check, message) _Static_assert(check, message)
#endif


// maybe some visibility things later, but I just removed the static inline
#define ZVEC_FUN_ATTRIBUTES 

#define ZVEC_TYPENAME(TypeName) zvec_##TypeName

#define ZVEC_ELEMENT_TYPENAME(Name) zvec_T_##Name

#define ZVEC_DEFINE_VEC_TYPE(T) ZVEC_DEFINE_VEC_TYPE_EXTENDED(T, T)

#define ZVEC_DEFINE_VEC_TYPE_EXTENDED(T, Name)                                                             \
typedef struct {                                                                            \
    T *data;                                                                                \
    size_t length;                                                                          \
    size_t capacity;                                                                        \
} ZVEC_TYPENAME(Name);                                                                            \
                                                                                            \
typedef T ZVEC_ELEMENT_TYPENAME(Name);                                                                    \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] ZVEC_TYPENAME(Name) zvec_init_capacity_##Name(size_t cap);                       \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] ZVEC_TYPENAME(Name) zvec_from_array_##Name(const T *arr, size_t count); \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] ZvecResult zvec_reserve_##Name(ZVEC_TYPENAME(Name) *v, size_t new_cap);                 \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T* zvec_push_slot_##Name(ZVEC_TYPENAME(Name) *v);               \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] ZvecResult zvec_push_##Name(ZVEC_TYPENAME(Name) *v, T value);                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] ZvecResult zvec_extend_##Name(ZVEC_TYPENAME(Name) *v, const T *items, size_t count);    \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_pop_##Name(ZVEC_TYPENAME(Name) *v);                                    \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T zvec_pop_get_##Name(ZVEC_TYPENAME(Name) *v);                                   \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_shrink_to_fit_##Name(ZVEC_TYPENAME(Name) *v);                          \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]]T zvec_at_##Name(ZVEC_TYPENAME(Name) v, size_t index);                          \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T* zvec_get_at_mut_##Name( ZVEC_TYPENAME(Name) *v, size_t index);                 \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]]  T const * zvec_get_at_##Name(const ZVEC_TYPENAME(Name) *v, size_t index);                 \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T* zvec_data_##Name(ZVEC_TYPENAME(Name) *v);                                     \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T* zvec_last_##Name(ZVEC_TYPENAME(Name) *v);                                     \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_remove_##Name(ZVEC_TYPENAME(Name) *v, size_t index);                   \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_swap_remove_##Name(ZVEC_TYPENAME(Name) *v, size_t index);              \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_clear_##Name(ZVEC_TYPENAME(Name) *v);                                  \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_free_##Name(ZVEC_TYPENAME(Name) *v);                                   \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_reverse_##Name(ZVEC_TYPENAME(Name) *v);                                \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_sort_##Name(ZVEC_TYPENAME(Name) *v,                                    \
                                                int (*compar)( T const *,  T const *));       \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T* zvec_bsearch_##Name(const ZVEC_TYPENAME(Name) *v, const T *key,                  \
                                 int (*compar)( T const *,  T const *));                      \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T* zvec_lower_bound_##Name(const ZVEC_TYPENAME(Name) *v,  T const *key,                        \
                                        int (*compar)( T const *,  T const *));

#define ZVEC_EMPTY(TypeName) ((ZVEC_TYPENAME(TypeName)){.data=NULL, .length=0, .capacity=0})

#define ZVEC_LENGTH(v) (v).length
#define ZVEC_IS_EMPTY(v) ((v).length == 0)

#define ZVEC_ASSERT_SHOULD_USE_PUSH(val) STATIC_ASSERT(sizeof(val) <= 8, "only small values should use push, use push slot for larger ones instead!")

#define ZVEC_ASSERT_SHOULD_USE_PUSH_SLOT(val) STATIC_ASSERT(sizeof(val) > 8, "only big values should use push slot, use push for smaller ones instead!")


#define ZVEC_PUSH(T, v, val)           ZVEC_PUSH_EXTENDED(T, T, v, val)         
#define ZVEC_PUSH_SLOT(T, v)           ZVEC_PUSH_SLOT_EXTENDED(T, T, v)         
#define ZVEC_EXTEND(T, v, arr, count)  ZVEC_EXTEND_EXTENDED(T, T, v, arr, count)
#define ZVEC_RESERVE(T, v, cap)        ZVEC_RESERVE_EXTENDED(T, T, v, cap)      
#define ZVEC_AT(T, v, idx)             ZVEC_AT_EXTENDED(T, T, v, idx)           
#define ZVEC_GET_AT(T, v, idx)         ZVEC_GET_AT_EXTENDED(T, T, v, idx)       
#define ZVEC_GET_AT_MUT(T, v, idx)     ZVEC_GET_AT_MUT_EXTENDED(T, T, v, idx)   
#define ZVEC_DATA(T, v)                ZVEC_DATA_EXTENDED(T, T, v)              
#define ZVEC_LAST(T, v)                ZVEC_LAST_EXTENDED(T, T, v)              
#define ZVEC_FREE(T, v)                ZVEC_FREE_EXTENDED(T, T, v)              
#define ZVEC_POP(T, v)                 ZVEC_POP_EXTENDED(T, T, v)               
#define ZVEC_POP_GET(T, v)             ZVEC_POP_GET_EXTENDED(T, T, v)           
#define ZVEC_SHRINK_TO_FIT(T, v)       ZVEC_SHRINK_TO_FIT_EXTENDED(T, T, v)     
#define ZVEC_REMOVE(T, v, i)           ZVEC_REMOVE_EXTENDED(T, T, v, i)         
#define ZVEC_SWAP_REMOVE(T, v, i)      ZVEC_SWAP_REMOVE_EXTENDED(T, T, v, i)    
#define ZVEC_CLEAR(T, v)               ZVEC_CLEAR_EXTENDED(T, T, v)             
#define ZVEC_REVERSE(T, v)             ZVEC_REVERSE_EXTENDED(T, T, v)           
#define ZVEC_SORT(T, v, cmp)           ZVEC_SORT_EXTENDED(T, T, v, cmp)         
#define ZVEC_BSEARCH(T, v, key, cmp)   ZVEC_BSEARCH_EXTENDED(T, T, v, key, cmp) 
#define ZVEC_LOWER_BOUND(T, v, key, cmp) ZVEC_LOWER_BOUND_EXTENDED(T, T, v, key, cmp) 
#define ZVEC_FROM(T, arr, size)         ZVEC_FROM_EXTENDED(T, T, arr, size)

#define ZVEC_PUSH_EXTENDED(T, Name, v, val)           zvec_push_##Name(v, val)
#define ZVEC_PUSH_SLOT_EXTENDED(T, Name, v)           zvec_push_slot_##Name(v)
#define ZVEC_EXTEND_EXTENDED(T, Name, v, arr, count)  zvec_extend_##Name(v, arr, count)
#define ZVEC_RESERVE_EXTENDED(T, Name, v, cap)        zvec_reserve_##Name(v, cap)
#define ZVEC_AT_EXTENDED(T, Name, v, idx)             zvec_at_##Name(v, idx)
#define ZVEC_GET_AT_EXTENDED(T, Name, v, idx)         zvec_get_at_##Name(v, idx)
#define ZVEC_GET_AT_MUT_EXTENDED(T, Name, v, idx)     zvec_get_at_mut_##Name(v, idx)
#define ZVEC_DATA_EXTENDED(T, Name, v)                zvec_data_##Name(v)
#define ZVEC_LAST_EXTENDED(T, Name, v)                zvec_last_##Name(v)
#define ZVEC_FREE_EXTENDED(T, Name, v)                zvec_free_##Name(v)
#define ZVEC_POP_EXTENDED(T, Name, v)                 zvec_pop_##Name(v)
#define ZVEC_POP_GET_EXTENDED(T, Name, v)             zvec_pop_get_##Name(v)
#define ZVEC_SHRINK_TO_FIT_EXTENDED(T, Name, v)       zvec_shrink_to_fit_##Name(v)
#define ZVEC_REMOVE_EXTENDED(T, Name, v, i)           zvec_remove_##Name(v, i)
#define ZVEC_SWAP_REMOVE_EXTENDED(T, Name, v, i)      zvec_swap_remove_##Name(v, i)
#define ZVEC_CLEAR_EXTENDED(T, Name, v)               zvec_clear_##Name(v)
#define ZVEC_REVERSE_EXTENDED(T, Name, v)             zvec_reverse_##Name(v)
#define ZVEC_SORT_EXTENDED(T, Name, v, cmp)           zvec_sort_##Name(v, cmp)
#define ZVEC_BSEARCH_EXTENDED(T, Name, v, key, cmp)   zvec_bsearch_##Name(v, key, cmp)
#define ZVEC_LOWER_BOUND_EXTENDED(T, Name, v, key, cmp) zvec_lower_bound_##Name(v, key, cmp)
#define ZVEC_FROM_EXTENDED(T, Name, arr, size)         zvec_from_array_##Name(arr, size)

#if !defined(Z_NO_GENERIC_USAGE)

#define ZVEC_PUSH_ENTRY(T, Name)       ZVEC_TYPENAME(Name)*: zvec_push_##Name,
#define ZVEC_PUSH_SLOT_ENTRY(T, Name)  ZVEC_TYPENAME(Name)*: zvec_push_slot_##Name,
#define ZVEC_EXTEND_ENTRY(T, Name)     ZVEC_TYPENAME(Name)*: zvec_extend_##Name,
#define ZVEC_RESERVE_ENTRY(T, Name)    ZVEC_TYPENAME(Name)*: zvec_reserve_##Name,
#define ZVEC_AT_ENTRY(T, Name)         ZVEC_TYPENAME(Name)*: zvec_at_##Name,
#define ZVEC_GET_AT_ENTRY(T, Name)     ZVEC_TYPENAME(Name)*: zvec_get_at_##Name,
#define ZVEC_GET_AT_MUT_ENTRY(T, Name) ZVEC_TYPENAME(Name)*: zvec_get_at_mut_##Name,
#define ZVEC_DATA_ENTRY(T, Name)       ZVEC_TYPENAME(Name)*: zvec_data_##Name,
#define ZVEC_LAST_ENTRY(T, Name)       ZVEC_TYPENAME(Name)*: zvec_last_##Name,
#define ZVEC_FREE_ENTRY(T, Name)       ZVEC_TYPENAME(Name)*: zvec_free_##Name,
#define ZVEC_POP_ENTRY(T, Name)        ZVEC_TYPENAME(Name)*: zvec_pop_##Name,
#define ZVEC_POP_GET_ENTRY(T, Name)    ZVEC_TYPENAME(Name)*: zvec_pop_get_##Name,
#define ZVEC_SHRINK_ENTRY(T, Name)     ZVEC_TYPENAME(Name)*: zvec_shrink_to_fit_##Name,
#define ZVEC_REMOVE_ENTRY(T, Name)     ZVEC_TYPENAME(Name)*: zvec_remove_##Name,
#define ZVEC_SWAP_REM_ENTRY(T, Name)   ZVEC_TYPENAME(Name)*: zvec_swap_remove_##Name,
#define ZVEC_CLEAR_ENTRY(T, Name)      ZVEC_TYPENAME(Name)*: zvec_clear_##Name,
#define ZVEC_REVERSE_ENTRY(T, Name)    ZVEC_TYPENAME(Name)*: zvec_reverse_##Name,
#define ZVEC_SORT_ENTRY(T, Name)       ZVEC_TYPENAME(Name)*: zvec_sort_##Name,
#define ZVEC_BSEARCH_ENTRY(T, Name)    ZVEC_TYPENAME(Name)*: zvec_bsearch_##Name,
#define ZVEC_LOWER_BOUND_ENTRY(T, Name) ZVEC_TYPENAME(Name)*: zvec_lower_bound_##Name,


#define zvec_push(v, val)          _Generic((v), REGISTER_TYPES(ZVEC_PUSH_ENTRY)      default: 0)      (v, val)
#define zvec_push_slot(v)          _Generic((v), REGISTER_TYPES(ZVEC_PUSH_SLOT_ENTRY) default: (void*)0)(v)
#define zvec_extend(v, arr, count) _Generic((v), REGISTER_TYPES(ZVEC_EXTEND_ENTRY)    default: 0)      (v, arr, count)
#define zvec_reserve(v, cap)       _Generic((v), REGISTER_TYPES(ZVEC_RESERVE_ENTRY)   default: 0)      (v, cap)
#define zvec_at(v, idx)            _Generic((v), REGISTER_TYPES(ZVEC_AT_ENTRY)        default: (void*)0)(v, idx)
#define zvec_get_at(v, idx)        _Generic((v), REGISTER_TYPES(ZVEC_GET_AT_ENTRY)    default: (void)0)(v, idx)
#define zvec_get_at_mut(v, idx)    _Generic((v), REGISTER_TYPES(ZVEC_GET_AT_MUT_ENTRY)default: (void)0)(v, idx)
#define zvec_data(v)               _Generic((v), REGISTER_TYPES(ZVEC_DATA_ENTRY)      default: (void*)0)(v)
#define zvec_last(v)               _Generic((v), REGISTER_TYPES(ZVEC_LAST_ENTRY)      default: (void*)0)(v)
#define zvec_free(v)               _Generic((v), REGISTER_TYPES(ZVEC_FREE_ENTRY)      default: (void)0)(v)
#define zvec_pop(v)                _Generic((v), REGISTER_TYPES(ZVEC_POP_ENTRY)       default: (void)0)(v)
#define zvec_pop_get(v)            _Generic((v), REGISTER_TYPES(ZVEC_POP_GET_ENTRY)   default: (void)0)(v)
#define zvec_shrink_to_fit(v)      _Generic((v), REGISTER_TYPES(ZVEC_SHRINK_ENTRY)    default: (void)0)(v)
#define zvec_remove(v, i)          _Generic((v), REGISTER_TYPES(ZVEC_REMOVE_ENTRY)    default: (void)0)(v, i)
#define zvec_swap_remove(v, i)     _Generic((v), REGISTER_TYPES(ZVEC_SWAP_REM_ENTRY)  default: (void)0)(v, i)
#define zvec_clear(v)              _Generic((v), REGISTER_TYPES(ZVEC_CLEAR_ENTRY)     default: (void)0)(v)
#define zvec_reverse(v)            _Generic((v), REGISTER_TYPES(ZVEC_REVERSE_ENTRY)   default: (void)0)(v)
#define zvec_sort(v, cmp)          _Generic((v), REGISTER_TYPES(ZVEC_SORT_ENTRY)      default: (void)0)(v, cmp)
#define zvec_bsearch(v, key, cmp)  _Generic((v), REGISTER_TYPES(ZVEC_BSEARCH_ENTRY)   default: (void*)0)(v, key, cmp)
#define zvec_lower_bound(v, k, c)  _Generic((v), REGISTER_TYPES(ZVEC_LOWER_BOUND_ENTRY) default: (void*)0)(v, k, c)

#endif

#define ZVEC_INIT(T) ZVEC_EMPTY(T)

#define ZVEC_INIT_WITH_CAP(Name, cap) zvec_init_capacity_##Name(cap)

#define ZVEC_CAT(a, b) a##b
#define ZVEC_NAME(a, b) ZVEC_CAT(a, b)

#define zvec_from(Name, ...) \
    zvec_from_array_##Name((ZVEC_ELEMENT_TYPENAME(Name)[])__VA_ARGS__, sizeof((ZVEC_ELEMENT_TYPENAME(Name)[])__VA_ARGS__) / sizeof(ZVEC_ELEMENT_TYPENAME(Name)))

#define zvec_foreach(v, iter) \
    for (size_t ZVEC_NAME(_i_, __LINE__) = 0; \
         ZVEC_NAME(_i_, __LINE__) < (v)->length && ((iter) = &(v)->data[ZVEC_NAME(_i_, __LINE__)]); \
         ++ZVEC_NAME(_i_, __LINE__))

#if Z_HAS_CLEANUP
    #define zvec_autofree(Name)  Z_CLEANUP(zvec_free_##Name) vec_##Name
#endif

#define ZVEC_IMPLEMENT_VEC_TYPE(T) ZVEC_IMPLEMENT_VEC_TYPE_EXTENDED(T, T)

#define ZVEC_IMPLEMENT_VEC_TYPE_EXTENDED(T, Name)                                                          \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] ZVEC_TYPENAME(Name) zvec_init_capacity_##Name(size_t cap) {                      \
    ZVEC_TYPENAME(Name) v = ZVEC_EMPTY(Name);                                                                  \
    if (cap > 0) {                                                                          \
        v.data = Z_VEC_CALLOC(cap, sizeof(T));                                                    \
        v.capacity = v.data ? cap : 0;                                                      \
    }                                                                                       \
    return v;                                                                               \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] ZVEC_TYPENAME(Name) zvec_from_array_##Name(const T * const arr, size_t count){                \
    ZVEC_TYPENAME(Name) v = zvec_init_capacity_##Name(count);                                         \
    if (v.data) {                                                                           \
        memcpy(v.data, arr, count * sizeof(T));                                             \
        v.length = count;                                                                   \
    }                                                                                       \
    return v;                                                                               \
}                                                                                           \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] ZvecResult zvec_reserve_##Name(ZVEC_TYPENAME(Name) *v, size_t new_cap) {                \
    if (new_cap <= v->capacity) return ZvecResultOk;                                              \
    T *new_data = Z_VEC_REALLOC(v->data, new_cap * sizeof(T));                                    \
    if (!new_data) return ZvecResultErr;                                                          \
    v->data = new_data;                                                                     \
    v->capacity = new_cap;                                                                  \
    return ZvecResultOk;                                                                          \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]]  T* zvec_push_slot_##Name(ZVEC_TYPENAME(Name) *v) {                                      \
    if (v->length >= v->capacity) {                                                         \
        size_t new_cap = v->capacity == 0 ? 8 : v->capacity * 2;                            \
        if (zvec_reserve_##Name(v, new_cap) != ZvecResultOk) return NULL;                          \
    }                                                                                       \
    return &v->data[v->length++];                                                           \
}                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] ZvecResult zvec_push_##Name(ZVEC_TYPENAME(Name) *v, T value) {                          \
    T *slot = zvec_push_slot_##Name(v);                                                      \
    if (!slot) {return ZvecResultErr; }                                                              \
    *slot = value;                                                                          \
    return ZvecResultOk;                                                                    \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] ZvecResult zvec_extend_##Name(ZVEC_TYPENAME(Name) *v, const T * const items, size_t count) {   \
    if (v->length + count > v->capacity) {                                                  \
        size_t new_cap = v->capacity == 0 ? 8 : v->capacity;                                \
        while (new_cap < v->length + count) new_cap *= 2;                                   \
        if (zvec_reserve_##Name(v, new_cap) != ZvecResultOk) return ZvecResultErr;                          \
    }                                                                                       \
    memcpy(v->data + v->length, items, count * sizeof(T));                                  \
    v->length += count;                                                                     \
    return ZvecResultOk;                                                                          \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_pop_##Name(ZVEC_TYPENAME(Name) *v) {                                   \
    assert(v->length > 0 && "Popping empty vector");                                        \
    v->length--;                                                                            \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T zvec_pop_get_##Name(ZVEC_TYPENAME(Name) *v) {                                  \
    assert(v->length > 0 && "Vector is empty, cannot pop!");                                \
    return v->data[--v->length];                                                            \
}                                                                                           \
ZVEC_FUN_ATTRIBUTES void zvec_shrink_to_fit_##Name(ZVEC_TYPENAME(Name) *v) {                         \
    if (v->length == 0) {                                                                   \
        Z_VEC_FREE(v->data);                                                                      \
        *v = ZVEC_EMPTY(Name);                                                            \
        return;                                                                             \
    }                                                                                       \
    if (v->length == v->capacity) return;                                                   \
    T *new_data = Z_VEC_REALLOC(v->data, v->length * sizeof(T));                                  \
    if (!new_data) return;                                                                  \
    v->data = new_data;                                                                     \
    v->capacity = v->length;                                                                \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T zvec_at_##Name(ZVEC_TYPENAME(Name) v, size_t index) {                         \
    assert(index < v.length && "Vector index out of bounds!");                             \
    return v.data[index];                                                                  \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T* zvec_get_at_mut_##Name( ZVEC_TYPENAME(Name) *v, size_t index){ \
    return (index < v->length) ? &v->data[index] : NULL;                                    \
}                                                                                            \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]]  T const * zvec_get_at_##Name( ZVEC_TYPENAME(Name) const * const v, size_t index){ \
    return (index < v->length) ? &v->data[index] : NULL;                                    \
}                                                                                            \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T* zvec_data_##Name(ZVEC_TYPENAME(Name) *v) {                                    \
    return v->data;                                                                         \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T* zvec_last_##Name(ZVEC_TYPENAME(Name) *v) {                                    \
    return (v->length > 0) ? &v->data[v->length - 1] : NULL;                                \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_remove_##Name(ZVEC_TYPENAME(Name) *v, size_t index) {                  \
    if (index >= v->length) return;                                                         \
    memmove(&v->data[index], &v->data[index + 1],                                           \
            (v->length - index - 1) * sizeof(T));                                           \
    v->length--;                                                                            \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_swap_remove_##Name(ZVEC_TYPENAME(Name) *v, size_t index) {             \
    if (index >= v->length) return;                                                         \
    v->data[index] = v->data[--v->length];                                                  \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_clear_##Name(ZVEC_TYPENAME(Name) *v) {                                 \
    v->length = 0;                                                                          \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_free_##Name(ZVEC_TYPENAME(Name) *v) {                                  \
    Z_VEC_FREE(v->data);                                                                          \
    *v = ZVEC_EMPTY(Name);                                                                \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_reverse_##Name(ZVEC_TYPENAME(Name) *v) {                               \
    if (v->length < 2) return;                                                              \
    size_t i = 0, j = v->length - 1;                                                        \
    while (i < j) {                                                                         \
        T temp = v->data[i];                                                                \
        v->data[i] = v->data[j];                                                            \
        v->data[j] = temp;                                                                  \
        i++; j--;                                                                           \
    }                                                                                       \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES void zvec_sort_##Name(ZVEC_TYPENAME(Name) *v,                                    \
                                        int (*compar)( T const *,  T const *)) {              \
    if (v->length > 1) {                                                                    \
        int (*qsort_cmp)(const void *, const void *) =                                      \
            (int (*)(const void *, const void *))compar;                                    \
        qsort(v->data, v->length, sizeof(T), qsort_cmp);                                    \
    }                                                                                       \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T* zvec_bsearch_##Name(const ZVEC_TYPENAME(Name) * const v, const T *key,                  \
                                 int (*compar)( T const *,  T const *)) {                     \
    if (v->length == 0) return NULL;                                                        \
    int (*bs_cmp)(const void *, const void *) =                                             \
        (int (*)(const void *, const void *))compar;                                        \
    return (T*) bsearch(key, v->data, v->length, sizeof(T), bs_cmp);                        \
}                                                                                           \
                                                                                            \
ZVEC_FUN_ATTRIBUTES [[nodiscard]] T* zvec_lower_bound_##Name(const ZVEC_TYPENAME(Name) * const v, T const *key,                        \
                                        int (*compar)(T const *,  T const *)) {              \
    size_t l = 0;                                                                           \
    size_t r = v->length;                                                                   \
    while (l < r) {                                                                         \
        size_t mid = l + (r - l) / 2;                                                       \
        if (compar(&v->data[mid], key) < 0) {                                               \
            l = mid + 1;                                                                    \
        } else {                                                                            \
            r = mid;                                                                        \
        }                                                                                   \
    }                                                                                       \
    if (l == v->length) { return NULL; }                                                    \
    return &v->data[l];                                                                     \
}


#define ZVEC_DEFINE_AND_IMPLEMENT_VEC_TYPE(T) \
    ZVEC_DEFINE_VEC_TYPE(T)                   \
    ZVEC_IMPLEMENT_VEC_TYPE(T)

#define ZVEC_DEFINE_AND_IMPLEMENT_VEC_TYPE_EXTENDED(T, Name) \
    ZVEC_DEFINE_VEC_TYPE_EXTENDED(T, Name)                   \
    ZVEC_IMPLEMENT_VEC_TYPE_EXTENDED(T, Name)

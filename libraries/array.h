/** 
 * Copyright (c) 2014 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _ARRAY_H
#define _ARRAY_H

#include <stdlib.h>
#include <string.h>

#define array_unpack_(v) \
    (char **)&(v)->data, &(v)->length, &(v)->capacity, sizeof(*(v)->data), &(v)->is_sized

#define array_t(T, N) \
    struct N {                \
        T *data;              \
        int length, capacity, is_sized; \
    };

#define array_init(v) \
    memset((v), 0, sizeof(*(v)))

#define array_init_sized(v, sz) \
    array_init_sized_(array_unpack_(v), sz)

#define array_deinit(v) \
    (free((v)->data),   \
     array_init(v))

#define array_push(v, val) \
    array_expand_(array_unpack_(v)) ? -1 : ((v)->data[(v)->length++] = (val), 0)

#define array_pop(v) \
    (v)->data[--(v)->length]

#define array_splice(v, start, count)               \
    (array_splice_(array_unpack_(v), start, count), \
     (v)->length -= (count))

#define array_swapsplice(v, start, count)               \
    (array_swapsplice_(array_unpack_(v), start, count), \
     (v)->length -= (count))

#define array_insert(v, idx, val) \
    (array_insert_(array_unpack_(v), idx) ? -1 : ((v)->data[idx] = (val), 0), (v)->length++, 0)

#define array_sort(v, fn) \
    qsort((v)->data, (v)->length, sizeof(*(v)->data), fn)

#define array_swap(v, idx1, idx2) \
    array_swap_(array_unpack_(v), idx1, idx2)

#define array_truncate(v, len) \
    ((v)->length = (len) < (v)->length ? (len) : (v)->length)

#define array_clear(v) \
    ((v)->length = 0)

#define array_first(v) \
    (v)->data[0]

#define array_get(v, i) \
    (v)->data[i]

#define array_last(v) \
    (v)->data[(v)->length - 1]

#define array_reserve(v, n) \
    array_reserve_(array_unpack_(v), n)

#define array_compact(v) \
    array_compact_(array_unpack_(v))

#define array_pusharr(v, arr, count)                                             \
    do {                                                                         \
        int i__, n__ = (count);                                                  \
        if (array_reserve_po2_(array_unpack_(v), (v)->length + n__) != 0) break; \
        for (i__ = 0; i__ < n__; i__++) {                                        \
            (v)->data[(v)->length++] = (arr)[i__];                               \
        }                                                                        \
    } while (0)

#define array_extend(v, v2) \
    array_pusharr((v), (v2)->data, (v2)->length)

#define array_find(v, val, idx)                         \
    do {                                                \
        for ((idx) = 0; (idx) < (v)->length; (idx)++) { \
            if ((v)->data[(idx)] == (val)) break;       \
        }                                               \
        if ((idx) == (v)->length) (idx) = -1;           \
    } while (0)

#define array_remove(v, val)                        \
    do {                                            \
        int idx__;                                  \
        array_find(v, val, idx__);                  \
        if (idx__ != -1) array_splice(v, idx__, 1); \
    } while (0)

#define array_reverse(v)                                   \
    do {                                                   \
        int i__ = (v)->length / 2;                         \
        while (i__--) {                                    \
            array_swap((v), i__, (v)->length - (i__ + 1)); \
        }                                                  \
    } while (0)

#define array_foreach(v, var, iter)                                    \
    if ((v)->length > 0)                                               \
        for ((iter) = 0;                                               \
             (iter) < (v)->length && (((var) = (v)->data[(iter)]), 1); \
             ++(iter))

#define array_foreach_rev(v, var, iter)                       \
    if ((v)->length > 0)                                      \
        for ((iter) = (v)->length - 1;                        \
             (iter) >= 0 && (((var) = (v)->data[(iter)]), 1); \
             --(iter))

#define array_foreach_ptr(v, var, iter)                                 \
    if ((v)->length > 0)                                                \
        for ((iter) = 0;                                                \
             (iter) < (v)->length && (((var) = &(v)->data[(iter)]), 1); \
             ++(iter))

#define array_foreach_ptr_rev(v, var, iter)                    \
    if ((v)->length > 0)                                       \
        for ((iter) = (v)->length - 1;                         \
             (iter) >= 0 && (((var) = &(v)->data[(iter)]), 1); \
             --(iter))

void array_init_sized_(char **data, int *length, int *capacity, int memsz, int *is_sized, int sz);
int array_expand_(char **data, int *length, int *capacity, int memsz, int *is_sized);
int array_reserve_(char **data, int *length, int *capacity, int memsz, int *is_sized, int n);
int array_reserve_po2_(char **data, int *length, int *capacity, int memsz, int *is_sized,
                       int n);
int array_compact_(char **data, int *length, int *capacity, int memsz, int *is_sized);
int array_insert_(char **data, int *length, int *capacity, int memsz, int *is_sized,
                  int idx);
void array_splice_(char **data, int *length, int *capacity, int memsz, int *is_sized,
                   int start, int count);
void array_swapsplice_(char **data, int *length, int *capacity, int memsz, int *is_sized,
                       int start, int count);
void array_swap_(char **data, int *length, int *capacity, int memsz, int *is_sized,
                 int idx1, int idx2);

array_t(int, array_int)
array_t(float, array_float)
array_t(float*, array_float_ptr)
array_t(char, array_char)
array_t(char*, array_char_ptr)

#include <stdbool.h>
array_t(bool, array_bool)

#include "maths.h"
array_t(vec2, array_vec2)
array_t(struct array_vec2, array_vec2_array)
array_t(vec3, array_vec3)
array_t(vec3*, array_vec3_ptr)
array_t(struct array_vec3, array_vec3_array)
array_t(vec4, array_vec4)
array_t(mat4, array_mat4)
array_t(struct array_mat4, array_mat4_array)
array_t(quat, array_quat)

#endif

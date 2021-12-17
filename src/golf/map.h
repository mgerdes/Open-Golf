/** 
 * Copyright (c) 2014 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _GOLF_MAP_H
#define _GOLF_MAP_H

#include <string.h>
#include "golf/alloc.h"

struct golf_map_node_t;
typedef struct golf_map_node_t golf_map_node_t;

typedef struct {
  golf_map_node_t **buckets;
  unsigned nbuckets, nnodes;
} golf_map_base_t;

typedef struct {
  unsigned bucketidx;
  golf_map_node_t *node;
} golf_map_iter_t;


#define golf_map_t(T)\
  struct { golf_map_base_t base; T *ref; T tmp; const char *alloc_category; }


#define golf_map_init(m, in_alloc_category)\
  ( memset(m, 0, sizeof(*(m))), (m)->alloc_category = (in_alloc_category) )


#define golf_map_deinit(m)\
  golf_map_deinit_(&(m)->base)


#define golf_map_get(m, key)\
  ( (m)->ref = golf_map_get_(&(m)->base, key) )


#define golf_map_set(m, key, value)\
  ( (m)->tmp = (value),\
    golf_map_set_(&(m)->base, key, &(m)->tmp, sizeof((m)->tmp), (m)->alloc_category) )


#define golf_map_remove(m, key)\
  golf_map_remove_(&(m)->base, key)


#define golf_map_iter(m)\
  golf_map_iter_()


#define golf_map_next(m, iter)\
  golf_map_next_(&(m)->base, iter)


void golf_map_deinit_(golf_map_base_t *m);
void *golf_map_get_(golf_map_base_t *m, const char *key);
int golf_map_set_(golf_map_base_t *m, const char *key, void *value, int vsize, const char *alloc_category);
void golf_map_remove_(golf_map_base_t *m, const char *key);
golf_map_iter_t golf_map_iter_(void);
const char *golf_map_next_(golf_map_base_t *m, golf_map_iter_t *iter);


typedef golf_map_t(void*) golf_map_void_t;
typedef golf_map_t(char*) golf_map_str_t;
typedef golf_map_t(int) golf_map_int_t;
typedef golf_map_t(char) golf_map_char_t;
typedef golf_map_t(float) golf_map_float_t;
typedef golf_map_t(double) golf_map_double_t;

#endif


/** 
 * Copyright (c) 2014 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>

#include "golf/map.h"
#include "golf/alloc.h"

struct golf_map_node_t {
  unsigned hash;
  void *value;
  golf_map_node_t *next;
  /* char key[]; */
  /* char value[]; */
};


static unsigned golf_map_hash(const char *str) {
  unsigned hash = 5381;
  while (*str) {
    hash = ((hash << 5) + hash) ^ *str++;
  }
  return hash;
}


static golf_map_node_t *golf_map_newnode(const char *key, void *value, int vsize, const char *alloc_category) {
  golf_map_node_t *node;
  int ksize = (int)strlen(key) + 1;
  int voffset = ksize + ((sizeof(void*) - ksize) % sizeof(void*));
  node = golf_alloc_tracked(sizeof(*node) + voffset + vsize, alloc_category);
  if (!node) return NULL;
  memcpy(node + 1, key, ksize);
  node->hash = golf_map_hash(key);
  node->value = ((char*) (node + 1)) + voffset;
  memcpy(node->value, value, vsize);
  return node;
}


static int golf_map_bucketidx(golf_map_base_t *m, unsigned hash) {
  /* If the implementation is changed to allow a non-power-of-2 bucket count,
   * the line below should be changed to use mod instead of AND */
  return hash & (m->nbuckets - 1);
}


static void golf_map_addnode(golf_map_base_t *m, golf_map_node_t *node) {
  int n = golf_map_bucketidx(m, node->hash);
  node->next = m->buckets[n];
  m->buckets[n] = node;
}


static int golf_map_resize(golf_map_base_t *m, int nbuckets, const char *alloc_category) {
  golf_map_node_t *nodes, *node, *next;
  golf_map_node_t **buckets;
  int i; 
  /* Chain all nodes together */
  nodes = NULL;
  i = m->nbuckets;
  while (i--) {
    node = (m->buckets)[i];
    while (node) {
      next = node->next;
      node->next = nodes;
      nodes = node;
      node = next;
    }
  }
  /* Reset buckets */
  buckets = golf_realloc_tracked(m->buckets, sizeof(*m->buckets) * nbuckets, alloc_category);
  if (buckets != NULL) {
    m->buckets = buckets;
    m->nbuckets = nbuckets;
  }
  if (m->buckets) {
    memset(m->buckets, 0, sizeof(*m->buckets) * m->nbuckets);
    /* Re-add nodes to buckets */
    node = nodes;
    while (node) {
      next = node->next;
      golf_map_addnode(m, node);
      node = next;
    }
  }
  /* Return error code if realloc() failed */
  return (buckets == NULL) ? -1 : 0;
}


static golf_map_node_t **golf_map_getref(golf_map_base_t *m, const char *key) {
  unsigned hash = golf_map_hash(key);
  golf_map_node_t **next;
  if (m->nbuckets > 0) {
    next = &m->buckets[golf_map_bucketidx(m, hash)];
    while (*next) {
      if ((*next)->hash == hash && !strcmp((char*) (*next + 1), key)) {
        return next;
      }
      next = &(*next)->next;
    }
  }
  return NULL;
}


void golf_map_deinit_(golf_map_base_t *m) {
  golf_map_node_t *next, *node;
  int i;
  i = m->nbuckets;
  while (i--) {
    node = m->buckets[i];
    while (node) {
      next = node->next;
      golf_free(node);
      node = next;
    }
  }
  golf_free(m->buckets);
}


void *golf_map_get_(golf_map_base_t *m, const char *key) {
  golf_map_node_t **next = golf_map_getref(m, key);
  return next ? (*next)->value : NULL;
}


int golf_map_set_(golf_map_base_t *m, const char *key, void *value, int vsize, const char *alloc_category) {
  int n, err;
  golf_map_node_t **next, *node;
  /* Find & replace existing node */
  next = golf_map_getref(m, key);
  if (next) {
    memcpy((*next)->value, value, vsize);
    return 0;
  }
  /* Add new node */
  node = golf_map_newnode(key, value, vsize, alloc_category);
  if (node == NULL) goto fail;
  if (m->nnodes >= m->nbuckets) {
    n = (m->nbuckets > 0) ? (m->nbuckets << 1) : 1;
    err = golf_map_resize(m, n, alloc_category);
    if (err) goto fail;
  }
  golf_map_addnode(m, node);
  m->nnodes++;
  return 0;
  fail:
  if (node) golf_free(node);
  return -1;
}


void golf_map_remove_(golf_map_base_t *m, const char *key) {
  golf_map_node_t *node;
  golf_map_node_t **next = golf_map_getref(m, key);
  if (next) {
    node = *next;
    *next = (*next)->next;
    golf_free(node);
    m->nnodes--;
  }
}


golf_map_iter_t golf_map_iter_(void) {
  golf_map_iter_t iter;
  iter.bucketidx = -1;
  iter.node = NULL;
  return iter;
}


const char *golf_map_next_(golf_map_base_t *m, golf_map_iter_t *iter) {
  if (iter->node) {
    iter->node = iter->node->next;
    if (iter->node == NULL) goto nextBucket;
  } else {
    nextBucket:
    do {
      if (++iter->bucketidx >= m->nbuckets) {
        return NULL;
      }
      iter->node = m->buckets[iter->bucketidx];
    } while (iter->node == NULL);
  }
  return (char*) (iter->node + 1);
}


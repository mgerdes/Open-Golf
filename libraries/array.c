/** 
 * Copyright (c) 2014 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <assert.h>

#include "array.h"

void array_init_sized_(char **data, int *length, int *capacity, int memsz, int *is_sized, int sz) {
    *data = malloc(memsz * sz);
    *length = 0;
    *capacity = sz;
    *is_sized = true;
}

int array_expand_(char **data, int *length, int *capacity, int memsz, int *is_sized) {
  if (*length + 1 > *capacity) {
    if (*is_sized) {
      assert(false);
      return -1;
    }
    void *ptr;
    int n = (*capacity == 0) ? 1 : *capacity << 1;
    ptr = realloc(*data, n * memsz);
    if (ptr == NULL) return -1;
    *data = ptr;
    *capacity = n;
  }
  return 0;
}


int array_reserve_(char **data, int *length, int *capacity, int memsz, int *is_sized, int n) {
  (void) length;
  if (n > *capacity) {
    if (*is_sized) {
      assert(false);
      return -1;
    }
    void *ptr = realloc(*data, n * memsz);
    if (ptr == NULL) return -1;
    *data = ptr;
    *capacity = n;
  }
  return 0;
}


int array_reserve_po2_(
  char **data, int *length, int *capacity, int memsz, int *is_sized, int n
) {
  int n2 = 1;
  if (n == 0) return 0;
  while (n2 < n) n2 <<= 1;
  return array_reserve_(data, length, capacity, memsz, is_sized, n2);
}


int array_compact_(char **data, int *length, int *capacity, int memsz, int *is_sized) {
  if (*length == 0) {
    free(*data);
    *data = NULL;
    *capacity = 0;
    return 0;
  } else {
    void *ptr;
    int n = *length;
    ptr = realloc(*data, n * memsz);
    if (ptr == NULL) return -1;
    *capacity = n;
    *data = ptr;
  }
  return 0;
}


int array_insert_(char **data, int *length, int *capacity, int memsz, int *is_sized,
                 int idx
) {
  int err = array_expand_(data, length, capacity, memsz, is_sized);
  if (err) return err;
  memmove(*data + (idx + 1) * memsz,
          *data + idx * memsz,
          (*length - idx) * memsz);
  return 0;
}


void array_splice_(char **data, int *length, int *capacity, int memsz, int *is_sized,
                 int start, int count
) {
  (void) capacity;
  memmove(*data + start * memsz,
          *data + (start + count) * memsz,
          (*length - start - count) * memsz);
}


void array_swapsplice_(char **data, int *length, int *capacity, int memsz, int *is_sized,
                     int start, int count
) {
  (void) capacity;
  memmove(*data + start * memsz,
          *data + (*length - count) * memsz,
          count * memsz);
}


void array_swap_(char **data, int *length, int *capacity, int memsz, int *is_sized,
               int idx1, int idx2 
) {
  unsigned char *a, *b, tmp;
  int count;
  (void) length;
  (void) capacity;
  if (idx1 == idx2) return;
  a = (unsigned char*) *data + idx1 * memsz;
  b = (unsigned char*) *data + idx2 * memsz;
  count = memsz;
  while (count--) {
    tmp = *a;
    *a = *b;
    *b = tmp;
    a++, b++;
  }
}



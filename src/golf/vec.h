#ifndef _GOLF_VEC_H
#define _GOLF_VEC_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "golf/alloc.h"

#define vec_unpack_(v)\
  (char**)&(v)->data, &(v)->length, &(v)->capacity, sizeof(*(v)->data)


#define vec_t(T)\
  struct { T *data; int length, capacity; const char *alloc_category; }


#define vec_init(v, in_alloc_category)\
  ( memset((v), 0, sizeof(*(v))), (v)->alloc_category = (in_alloc_category) )


#define vec_deinit(v)\
  ( golf_free((v)->data) )


#define vec_push(v, val)\
  ( vec_expand_(vec_unpack_(v), (v)->alloc_category) ? -1 :\
    ((v)->data[(v)->length++] = (val), 0) )


#define vec_pop(v)\
  (v)->data[--(v)->length]


#define vec_splice(v, start, count)\
  ( vec_splice_(vec_unpack_(v), start, count),\
    (v)->length -= (count) )


#define vec_swapsplice(v, start, count)\
  ( vec_swapsplice_(vec_unpack_(v), start, count),\
    (v)->length -= (count) )


#define vec_insert(v, idx, val)\
  ( vec_insert_(vec_unpack_(v), idx, (v)->alloc_category) ? -1 :\
    ((v)->data[idx] = (val), 0), (v)->length++, 0 )
    

#define vec_sort(v, fn)\
  qsort((v)->data, (v)->length, sizeof(*(v)->data), fn)


#define vec_swap(v, idx1, idx2)\
  vec_swap_(vec_unpack_(v), idx1, idx2)


#define vec_truncate(v, len)\
  ((v)->length = (len) < (v)->length ? (len) : (v)->length)


#define vec_clear(v)\
  ((v)->length = 0)


#define vec_first(v)\
  (v)->data[0]


#define vec_last(v)\
  (v)->data[(v)->length - 1]


#define vec_reserve(v, n)\
  vec_reserve_(vec_unpack_(v), n, (v)->alloc_category)

 
#define vec_compact(v)\
  vec_compact_(vec_unpack_(v), (v)->alloc_category)


#define vec_pusharr(v, arr, count)\
  do {\
    int i__, n__ = (count);\
    if (vec_reserve_po2_(vec_unpack_(v), (v)->length + n__, (v)->alloc_category) != 0) break;\
    for (i__ = 0; i__ < n__; i__++) {\
      (v)->data[(v)->length++] = (arr)[i__];\
    }\
  } while (0)


#define vec_extend(v, v2)\
  vec_pusharr((v), (v2)->data, (v2)->length)


#define vec_find(v, val, idx)\
  do {\
    for ((idx) = 0; (idx) < (v)->length; (idx)++) {\
      if ((v)->data[(idx)] == (val)) break;\
    }\
    if ((idx) == (v)->length) (idx) = -1;\
  } while (0)


#define vec_remove(v, val)\
  do {\
    int idx__;\
    vec_find(v, val, idx__);\
    if (idx__ != -1) vec_splice(v, idx__, 1);\
  } while (0)


#define vec_reverse(v)\
  do {\
    int i__ = (v)->length / 2;\
    while (i__--) {\
      vec_swap((v), i__, (v)->length - (i__ + 1));\
    }\
  } while (0)


#define vec_foreach(v, var, iter)\
  if  ( (v)->length > 0 )\
  for ( (iter) = 0;\
        (iter) < (v)->length && (((var) = (v)->data[(iter)]), 1);\
        ++(iter))


#define vec_foreach_rev(v, var, iter)\
  if  ( (v)->length > 0 )\
  for ( (iter) = (v)->length - 1;\
        (iter) >= 0 && (((var) = (v)->data[(iter)]), 1);\
        --(iter))


#define vec_foreach_ptr(v, var, iter)\
  if  ( (v)->length > 0 )\
  for ( (iter) = 0;\
        (iter) < (v)->length && (((var) = &(v)->data[(iter)]), 1);\
        ++(iter))


#define vec_foreach_ptr_rev(v, var, iter)\
  if  ( (v)->length > 0 )\
  for ( (iter) = (v)->length - 1;\
        (iter) >= 0 && (((var) = &(v)->data[(iter)]), 1);\
        --(iter))



int vec_expand_(char **data, int *length, int *capacity, int memsz, const char *alloc_category);
int vec_reserve_(char **data, int *length, int *capacity, int memsz, int n, const char *alloc_category);
int vec_reserve_po2_(char **data, int *length, int *capacity, int memsz,
                     int n, const char *alloc_category);
int vec_compact_(char **data, int *length, int *capacity, int memsz, const char *alloc_category);
int vec_insert_(char **data, int *length, int *capacity, int memsz,
                int idx, const char *alloc_category);
void vec_splice_(char **data, int *length, int *capacity, int memsz,
                 int start, int count);
void vec_swapsplice_(char **data, int *length, int *capacity, int memsz,
                     int start, int count);
void vec_swap_(char **data, int *length, int *capacity, int memsz,
               int idx1, int idx2);


typedef vec_t(void*) vec_void_t;
typedef vec_t(char*) vec_str_t;
typedef vec_t(char*) vec_char_ptr_t;
typedef vec_t(int) vec_int_t;
typedef vec_t(char) vec_char_t;
typedef vec_t(float) vec_float_t;
typedef vec_t(double) vec_double_t;
typedef vec_t(bool) vec_bool_t;

#endif

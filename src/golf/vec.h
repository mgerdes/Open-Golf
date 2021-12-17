#ifndef _GOLF_VEC_H
#define _GOLF_VEC_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "golf/alloc.h"

#define golf_vec_unpack_(v)\
  (char**)&(v)->data, &(v)->length, &(v)->capacity, sizeof(*(v)->data)


#define golf_vec_t(T)\
  struct { T *data; int length, capacity; const char *alloc_category; }


#define golf_vec_init(v, alloc_category)\
  memset((v), 0, sizeof(*(v)))


#define golf_vec_deinit(v)\
  ( golf_free((v)->data),\
    golf_vec_init(v) ) 


#define golf_vec_push(v, val)\
  ( golf_vec_expand_(golf_vec_unpack_(v), (v)->alloc_category) ? -1 :\
    ((v)->data[(v)->length++] = (val), 0), 0 )


#define golf_vec_pop(v)\
  (v)->data[--(v)->length]


#define golf_vec_splice(v, start, count)\
  ( golf_vec_splice_(golf_vec_unpack_(v), start, count),\
    (v)->length -= (count) )


#define golf_vec_swapsplice(v, start, count)\
  ( golf_vec_swapsplice_(golf_vec_unpack_(v), start, count),\
    (v)->length -= (count) )


#define golf_vec_insert(v, idx, val)\
  ( golf_vec_insert_(golf_vec_unpack_(v), idx, (v)->alloc_category) ? -1 :\
    ((v)->data[idx] = (val), 0), (v)->length++, 0 )
    

#define golf_vec_sort(v, fn)\
  qsort((v)->data, (v)->length, sizeof(*(v)->data), fn)


#define golf_vec_swap(v, idx1, idx2)\
  golf_vec_swap_(golf_vec_unpack_(v), idx1, idx2)


#define golf_vec_truncate(v, len)\
  ((v)->length = (len) < (v)->length ? (len) : (v)->length)


#define golf_vec_clear(v)\
  ((v)->length = 0)


#define golf_vec_first(v)\
  (v)->data[0]


#define golf_vec_last(v)\
  (v)->data[(v)->length - 1]


#define golf_vec_reserve(v, n)\
  golf_vec_reserve_(golf_vec_unpack_(v), n, (v)->alloc_category)

 
#define golf_vec_compact(v)\
  golf_vec_compact_(golf_vec_unpack_(v), (v)->alloc_category)


#define golf_vec_pusharr(v, arr, count)\
  do {\
    int i__, n__ = (count);\
    if (golf_vec_reserve_po2_(golf_vec_unpack_(v), (v)->length + n__, (v)->alloc_category) != 0) break;\
    for (i__ = 0; i__ < n__; i__++) {\
      (v)->data[(v)->length++] = (arr)[i__];\
    }\
  } while (0)


#define golf_vec_extend(v, v2)\
  golf_vec_pusharr((v), (v2)->data, (v2)->length)


#define golf_vec_find(v, val, idx)\
  do {\
    for ((idx) = 0; (idx) < (v)->length; (idx)++) {\
      if ((v)->data[(idx)] == (val)) break;\
    }\
    if ((idx) == (v)->length) (idx) = -1;\
  } while (0)


#define golf_vec_remove(v, val)\
  do {\
    int idx__;\
    golf_vec_find(v, val, idx__);\
    if (idx__ != -1) golf_vec_splice(v, idx__, 1);\
  } while (0)


#define golf_vec_reverse(v)\
  do {\
    int i__ = (v)->length / 2;\
    while (i__--) {\
      golf_vec_swap((v), i__, (v)->length - (i__ + 1));\
    }\
  } while (0)


#define golf_vec_foreach(v, var, iter)\
  if  ( (v)->length > 0 )\
  for ( (iter) = 0;\
        (iter) < (v)->length && (((var) = (v)->data[(iter)]), 1);\
        ++(iter))


#define golf_vec_foreach_rev(v, var, iter)\
  if  ( (v)->length > 0 )\
  for ( (iter) = (v)->length - 1;\
        (iter) >= 0 && (((var) = (v)->data[(iter)]), 1);\
        --(iter))


#define golf_vec_foreach_ptr(v, var, iter)\
  if  ( (v)->length > 0 )\
  for ( (iter) = 0;\
        (iter) < (v)->length && (((var) = &(v)->data[(iter)]), 1);\
        ++(iter))


#define golf_vec_foreach_ptr_rev(v, var, iter)\
  if  ( (v)->length > 0 )\
  for ( (iter) = (v)->length - 1;\
        (iter) >= 0 && (((var) = &(v)->data[(iter)]), 1);\
        --(iter))



int golf_vec_expand_(char **data, int *length, int *capacity, int memsz, const char *alloc_category);
int golf_vec_reserve_(char **data, int *length, int *capacity, int memsz, int n, const char *alloc_category);
int golf_vec_reserve_po2_(char **data, int *length, int *capacity, int memsz,
                     int n, const char *alloc_category);
int golf_vec_compact_(char **data, int *length, int *capacity, int memsz, const char *alloc_category);
int golf_vec_insert_(char **data, int *length, int *capacity, int memsz,
                int idx, const char *alloc_category);
void golf_vec_splice_(char **data, int *length, int *capacity, int memsz,
                 int start, int count);
void golf_vec_swapsplice_(char **data, int *length, int *capacity, int memsz,
                     int start, int count);
void golf_vec_swap_(char **data, int *length, int *capacity, int memsz,
               int idx1, int idx2);


typedef golf_vec_t(void*) golf_vec_void_t;
typedef golf_vec_t(char*) golf_vec_str_t;
typedef golf_vec_t(char*) golf_vec_char_ptr_t;
typedef golf_vec_t(int) golf_vec_int_t;
typedef golf_vec_t(char) golf_vec_char_t;
typedef golf_vec_t(float) golf_vec_float_t;
typedef golf_vec_t(double) golf_vec_double_t;
typedef golf_vec_t(bool) golf_vec_bool_t;

#endif

#ifndef _GOLF_STRING_H
#define _GOLF_STRING_H

#include "golf/allocator.h"

typedef struct golf_string {
    golf_allocator_t allocator;
    int cap, len;
    char *cstr;
} golf_string_t;

void golf_string_init(golf_string_t *str, golf_allocator_t allocator);
void golf_string_deinit(golf_string_t *str);
void golf_string_set_cstr(golf_string_t *str, const char *cstr);
void golf_string_append_char(golf_string_t *str, char c);
void golf_string_append_str(golf_string_t *str, golf_string_t *str2);
void golf_string_append_cstr(golf_string_t *str, const char *cstr);
void golf_string_append_cstr_len(golf_string_t *str, const char *cstr, int cstr_len);
void golf_string_appendf(golf_string_t *str, const char *format, ...);
void golf_string_pop(golf_string_t *str, int n);

#endif

#define _CRT_SECURE_NO_WARNINGS

#include "golf/string.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

static void _golf_string_grow(golf_string_t *str, int new_len) {
    if (str->cap < new_len) {
        char *old_cstr = str->cstr;  
        str->cap = 2 * (new_len + 1);
        str->cstr =  (str->allocator.alloc)(sizeof(char)*(str->cap + 1));
        if (old_cstr) {
            strcpy(str->cstr, old_cstr);
        }
        (str->allocator.free)(old_cstr);
    }
}

void golf_string_init(golf_string_t *str, golf_allocator_t allocator) {
    str->allocator = allocator;
    str->cap = 0;
    str->len = 0;
    str->cstr = NULL;
}

void golf_string_deinit(golf_string_t *str) {
    str->allocator.free(str->cstr);
}

void golf_string_set_cstr(golf_string_t *str, const char *cstr) {
    str->len = 0;
    golf_string_append_cstr(str, cstr);
}

void golf_string_append_char(golf_string_t *str, char c) {
    _golf_string_grow(str, str->len + 1);
    str->cstr[str->len] = c;
    str->cstr[str->len + 1] = '\0';
    str->len += 1; 
}

void golf_string_append_str(golf_string_t *str, golf_string_t *str2) {
    golf_string_append_cstr(str, str2->cstr);
}

void golf_string_append_cstr(golf_string_t *str, const char *cstr) {
    int cstr_len = (int)strlen(cstr);
    golf_string_append_cstr_len(str, cstr, cstr_len);
}

void golf_string_append_cstr_len(golf_string_t *str, const char *cstr, int cstr_len) {
    _golf_string_grow(str, str->len + cstr_len);
    strcat(str->cstr, cstr);
    str->len += cstr_len;
}

void golf_string_appendf(golf_string_t *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    _golf_string_grow(str, str->len + len);
    va_start(args, format);
    vsprintf(str->cstr + str->len, format, args);
    va_end(args);
    str->len += len;
}

void golf_string_pop(golf_string_t *str, int n) {
    if (str->len > n) {
        str->len -= n;
    }
    else {
        str->len = 0;
    }
    str->cstr[str->len] = 0;
}

#define _CRT_SECURE_NO_WARNINGS

#include "mstring.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

static void _mstring_grow(mstring_t *str, int new_len) {
    if (str->cap < new_len) {
        char *old_cstr = str->cstr;  
        str->cap = 2 * (new_len + 1);
        str->cstr =  malloc(sizeof(char)*(str->cap + 1));
        strcpy(str->cstr, old_cstr);
        free(old_cstr);
    }
}

void mstring_init(mstring_t *str, const char *cstr) {
    int len = (int)strlen(cstr);
    str->cap = len;
    str->len = len;
    str->cstr = malloc(sizeof(char)*(str->cap + 1));
    strcpy(str->cstr, cstr);
}

void mstring_initf(mstring_t *str, const char *format, ...) {
    va_list args; 
    va_start(args, format); 
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    str->cap = len;
    str->len = len;
    str->cstr = malloc(sizeof(char)*(str->cap + 1));

    va_start(args, format); 
    vsprintf(str->cstr, format, args);
    va_end(args);
}

void mstring_deinit(mstring_t *str) {
    free(str->cstr);
}

void mstring_append_char(mstring_t *str, char c) {
    _mstring_grow(str, str->len + 1);
    str->cstr[str->len] = c;
    str->cstr[str->len + 1] = '\0';
    str->len += 1; 
}

void mstring_append_str(mstring_t *str, mstring_t *str2) {
    mstring_append_cstr(str, str2->cstr);
}

void mstring_append_cstr(mstring_t *str, const char *cstr) {
    int cstr_len = (int)strlen(cstr);
    mstring_append_cstr_len(str, cstr, cstr_len);
}

void mstring_append_cstr_len(mstring_t *str, const char *cstr, int cstr_len) {
    _mstring_grow(str, str->len + cstr_len);
    strcat(str->cstr, cstr);
    str->len += cstr_len;
}

void mstring_appendf(mstring_t *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    _mstring_grow(str, str->len + len);
    va_start(args, format);
    vsprintf(str->cstr + str->len, format, args);
    va_end(args);
    str->len += len;
}

void mstring_pop(mstring_t *str, int n) {
    if (str->len > n) {
        str->len -= n;
    }
    else {
        str->len = 0;
    }
    str->cstr[str->len] = 0;
}

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

static const unsigned char _b64_table[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void mstring_base64_encode(mstring_t *str, const unsigned char *src, int len) {
    int i = 0;
    int j = 0;
    char *enc = NULL;
    size_t size = 0;
    unsigned char buf[4];
    unsigned char tmp[3];

    while (len--) {
        tmp[i++] = *(src++);

        if (3 == i) {
            buf[0] = (tmp[0] & 0xfc) >> 2;
            buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
            buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
            buf[3] = tmp[2] & 0x3f;

            for (i = 0; i < 4; ++i) {
                mstring_append_char(str, _b64_table[buf[i]]);
            }

            i = 0;
        }
    }

    if (i > 0) {
        for (j = i; j < 3; ++j) {
            tmp[j] = '\0';
        }

        // perform same codec as above
        buf[0] = (tmp[0] & 0xfc) >> 2;
        buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
        buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
        buf[3] = tmp[2] & 0x3f;

        for (j = 0; (j < i + 1); ++j) {
            mstring_append_char(str, _b64_table[buf[j]]);
        }

        while ((i++ < 3)) {
            mstring_append_char(str, '=');
        }
    }
}

#if 0

/**
 * `encode.c' - b64
 *
 * copyright (c) 2014 joseph werle
 */

#include <stdio.h>
#include <stdlib.h>
#include "b64.h"

#ifdef b64_USE_CUSTOM_MALLOC
extern void* b64_malloc(size_t);
#endif

#ifdef b64_USE_CUSTOM_REALLOC
extern void* b64_realloc(void*, size_t);
#endif

char *
b64_encode (const unsigned char *src, size_t len) {
  int i = 0;
  int j = 0;
  char *enc = NULL;
  size_t size = 0;
  unsigned char buf[4];
  unsigned char tmp[3];

  // alloc
  enc = (char *) b64_buf_malloc();
  if (NULL == enc) { return NULL; }

  // parse until end of source
  while (len--) {
    // read up to 3 bytes at a time into `tmp'
    tmp[i++] = *(src++);

    // if 3 bytes read then encode into `buf'
    if (3 == i) {
      buf[0] = (tmp[0] & 0xfc) >> 2;
      buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
      buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
      buf[3] = tmp[2] & 0x3f;

      // allocate 4 new byts for `enc` and
      // then translate each encoded buffer
      // part by index from the base 64 index table
      // into `enc' unsigned char array
      enc = (char *) b64_buf_realloc(enc, size + 4);
      for (i = 0; i < 4; ++i) {
        enc[size++] = b64_table[buf[i]];
      }

      // reset index
      i = 0;
    }
  }

  // remainder
  if (i > 0) {
    // fill `tmp' with `\0' at most 3 times
    for (j = i; j < 3; ++j) {
      tmp[j] = '\0';
    }

    // perform same codec as above
    buf[0] = (tmp[0] & 0xfc) >> 2;
    buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
    buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
    buf[3] = tmp[2] & 0x3f;

    // perform same write to `enc` with new allocation
    for (j = 0; (j < i + 1); ++j) {
      enc = (char *) b64_buf_realloc(enc, size + 1);
      enc[size++] = b64_table[buf[j]];
    }

    // while there is still a remainder
    // append `=' to `enc'
    while ((i++ < 3)) {
      enc = (char *) b64_buf_realloc(enc, size + 1);
      enc[size++] = '=';
    }
  }

  // Make sure we have enough space to add '\0' character at end.
  enc = (char *) b64_buf_realloc(enc, size + 1);
  enc[size] = '\0';

  return enc;
}

#endif

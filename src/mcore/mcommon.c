#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "mcommon.h"

void mstrncpy(char *dest, const char *src, int n) {
    strncpy(dest, src, n);
    dest[n - 1] = 0;
}

/*
    base64 encoding / decoding library based off https://github.com/littlstar/b64.c
*/

static const char _b64_table[] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
  'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', '0', '1', '2', '3',
  '4', '5', '6', '7', '8', '9', '+', '/'
};

static void _buf_resize(char** buf, int *buf_size, int new_size) {
    if (new_size > *buf_size) {
        while (*buf_size < new_size) *buf_size = (*buf_size + 1) * 2;
        *buf = realloc(*buf, *buf_size);
    }
}

unsigned char *mbase64_decode(const char *src, int len, int *out_dec_len) {
    int i = 0;
    int j = 0;
    int l = 0;
    int size = 0;
    unsigned char *dec = NULL;
    int dec_size = 1024;
    unsigned char buf[3];
    unsigned char tmp[4];

    // alloc
    *out_dec_len = 0;
    dec = malloc(dec_size);
    if (NULL == dec) { return NULL; }

    // parse until end of source
    while (len--) {
        // break if char is `=' or not base64 char
        if ('=' == src[j]) { break; }
        if (!(isalnum(src[j]) || '+' == src[j] || '/' == src[j])) { break; }

        // read up to 4 bytes at a time into `tmp'
        tmp[i++] = src[j++];

        // if 4 bytes read then decode into `buf'
        if (4 == i) {
            // translate values in `tmp' from table
            for (i = 0; i < 4; ++i) {
                // find translation char in `_b64_table'
                for (l = 0; l < 64; ++l) {
                    if (tmp[i] == _b64_table[l]) {
                        tmp[i] = l;
                        break;
                    }
                }
            }

            // decode
            buf[0] = (tmp[0] << 2) + ((tmp[1] & 0x30) >> 4);
            buf[1] = ((tmp[1] & 0xf) << 4) + ((tmp[2] & 0x3c) >> 2);
            buf[2] = ((tmp[2] & 0x3) << 6) + tmp[3];

            // write decoded buffer to `dec'
            _buf_resize((char**)&dec, &dec_size, size + 3);
            if (dec != NULL){
                for (i = 0; i < 3; ++i) {
                    dec[size++] = buf[i];
                }
            } else {
                return NULL;
            }

            // reset
            i = 0;
        }
    }

    // remainder
    if (i > 0) {
        // fill `tmp' with `\0' at most 4 times
        for (j = i; j < 4; ++j) {
            tmp[j] = '\0';
        }

        // translate remainder
        for (j = 0; j < 4; ++j) {
            // find translation char in `_b64_table'
            for (l = 0; l < 64; ++l) {
                if (tmp[j] == _b64_table[l]) {
                    tmp[j] = l;
                    break;
                }
            }
        }

        // decode remainder
        buf[0] = (tmp[0] << 2) + ((tmp[1] & 0x30) >> 4);
        buf[1] = ((tmp[1] & 0xf) << 4) + ((tmp[2] & 0x3c) >> 2);
        buf[2] = ((tmp[2] & 0x3) << 6) + tmp[3];

        // write remainer decoded buffer to `dec'
        _buf_resize((char**)&dec, &dec_size, size + (i - 1));
        if (dec != NULL){
            for (j = 0; (j < i - 1); ++j) {
                dec[size++] = buf[j];
            }
        } else {
            return NULL;
        }
    }

    // Make sure we have enough space to add '\0' character at end.
    _buf_resize((char**)&dec, &dec_size, size + 1);
    if (dec != NULL){
        dec[size] = '\0';
    } else {
        return NULL;
    }

    *out_dec_len = size;
    return dec;
}

char *mbase64_encode(const unsigned char *src, int len) {
    int i = 0;
    int j = 0;
    int enc_size = 1024;
    char *enc = NULL;
    int size = 0;
    unsigned char buf[4];
    unsigned char tmp[3];

    // alloc
    enc = malloc(enc_size);
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
            _buf_resize(&enc, &enc_size, size + 4);
            for (i = 0; i < 4; ++i) {
                enc[size++] = _b64_table[buf[i]];
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
            _buf_resize(&enc, &enc_size, size + 1);
            enc[size++] = _b64_table[buf[j]];
        }

        // while there is still a remainder
        // append `=' to `enc'
        while ((i++ < 3)) {
            _buf_resize(&enc, &enc_size, size + 1);
            enc[size++] = '=';
        }
    }

    // Make sure we have enough space to add '\0' character at end.
    _buf_resize(&enc, &enc_size, size + 1);
    enc[size] = '\0';

    return enc;
}

bool mwrite_file(const char *path, unsigned char *data, int data_len) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return false;
    }

    fwrite(data, sizeof(char), data_len, f);
    fclose(f);
    return true;
}

bool mread_file(const char *path, unsigned char **data, int *data_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }

    fseek(f, 0, SEEK_END);
    *data_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    *data = malloc(*data_len);
    int ret = (int) fread(*data, sizeof(unsigned char), *data_len, f);
    if (ret == -1) {
        fclose(f);
        free(*data);
        return false;
    }
    fclose(f);

    return true;
}

bool mstr_copy_line(char **str, char **line_buffer, int *line_buffer_cap) {
    char c0 = (*str)[0];
    if (!c0) {
        return false;
    }

    int i = 0;
    while (true) {
        char c = (*str)[i];
        if (c == '\n' || !c) {
            break;
        }

        if (i == *line_buffer_cap) {
            _buf_resize(line_buffer, line_buffer_cap, i + 1);
        }
        (*line_buffer)[i] = c;
        i++;
    }
    if (i > 0 && (*line_buffer)[i - 1] == '\r') {
        (*line_buffer)[i - 1] = 0;
    }

    // Allocate room for the null character if needed.
    if (i == *line_buffer_cap) {
        _buf_resize(line_buffer, line_buffer_cap, i + 1);
    }
    (*line_buffer)[i] = 0;
    *str += i;
    // Skip over the new line character if we need to
    if ((*str)[i]) {
        *str += 1;
    }

    return true;
}

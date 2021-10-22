#ifndef _GOLF_BASE64_H
#define _GOLF_BASE64_H

#include <stdbool.h>

unsigned char *golf_base64_decode(const char *src, int len, int *dec_len); 
char *golf_base64_encode(const unsigned char *src, int len);

#endif

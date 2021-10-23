#ifndef _GOLF_BASE64_H
#define _GOLF_BASE64_H

#include <stdbool.h>

int golf_base64_encode_out_len(const unsigned char *src, int len);
int golf_base64_decode_out_len(const unsigned char *src, int len);
bool golf_base64_encode(const unsigned char *src, int len, unsigned char *out);
bool golf_base64_decode(const unsigned char *src, int len, unsigned char *out);

#endif

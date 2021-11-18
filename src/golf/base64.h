#ifndef _GOLF_BASE64_H
#define _GOLF_BASE64_H

#include <stdbool.h>

/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005-2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

unsigned char *golf_base64_encode(const unsigned char *src, int len, int *out_len);
unsigned char *golf_base64_decode(const unsigned char *src, int len, int *out_len);

#endif

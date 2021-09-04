#ifndef BASE64_H
#define BASE64_H

/*
    base64 encoding / decoding library modified from https://github.com/littlstar/b64.c
*/

unsigned char *base64_decode(const char *src, int len, int *dec_len); 
char *base64_encode(const unsigned char *src, int len);

#endif

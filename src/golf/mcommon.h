#ifndef _MCOMMON_H
#define _MCOMMON_H

void mstrncpy(char *dest, const char *src, int n);
unsigned char *mbase64_decode(const char *src, int len, int *dec_len); 
char *mbase64_encode(const unsigned char *src, int len);

#endif

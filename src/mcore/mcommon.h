#ifndef _MCOMMON_H
#define _MCOMMON_H

#include <stdbool.h>

void mstrncpy(char *dest, const char *src, int n);
unsigned char *mbase64_decode(const char *src, int len, int *dec_len); 
char *mbase64_encode(const unsigned char *src, int len);
bool mwrite_file(const char *path, unsigned char *data, int data_len);
bool mread_file(const char *path, unsigned char **data, int *data_len);
bool mstr_copy_line(char **str, char **line_buffer, int *line_buffer_cap);

#endif

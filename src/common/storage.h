#ifndef _GOLF_STORAGE_H
#define _GOLF_STORAGE_H

#include <stdbool.h>

void golf_storage_init(void);
bool golf_storage_finish_init(void);
void golf_storage_set_num(const char *key, float num);
bool golf_storage_get_num(const char *key, float *num);
void golf_storage_save(void);

#endif

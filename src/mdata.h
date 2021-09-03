#ifndef _MDATA_H
#define _MDATA_H

#include <stdint.h>

#include "mfile.h"

struct mdata_file;
typedef struct mdata_file mdata_file_t;

void mdata_init(void);
void mdata_add_extension_handler(const char *ext, bool (*mdata_file_creator)(mfile_t files, mdata_file_t *mdata_files, void *udata), bool (*mdata_file_handler)(const char *file_path, mdata_file_t *mdata_file, void *udata), void *udata);

bool mdata_file_get_val_int(mdata_file_t *mdata_file, const char *field, int *val);
bool mdata_file_get_val_uint64(mdata_file_t *mdata_file, const char *field, uint64_t *val);
bool mdata_file_get_val_binary_data(mdata_file_t *mdata_file, const char *field, char **data, int *data_len);

void mdata_file_add_val_int(mdata_file_t *mdata_file, const char *field, int int_val, bool user_set);
void mdata_file_add_val_uint64(mdata_file_t *mdata_file, const char *field, uint64_t uint64_val, bool user_set);
void mdata_file_add_val_string(mdata_file_t *mdata_file, const char *field, const char *string_val, bool user_set);
void mdata_file_add_val_binary_data(mdata_file_t *mdata_file, const char *field, char *data, int data_len, bool compress);

#endif

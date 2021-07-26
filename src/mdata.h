#ifndef _MDATA_H
#define _MDATA_H

#include <stdint.h>

#include "mfile.h"

struct mdata_file;
typedef struct mdata_file mdata_file_t;

void mdata_init(void);
void mdata_add_extension(const char *ext, bool (*mdata_file_creator)(mfile_t file, mdata_file_t *mdata_file));

void mdata_file_add_val_int(mdata_file_t *mdata_file, const char *field, int val);
void mdata_file_add_val_uint64(mdata_file_t *mdata_file, const char *field, uint64_t val);
void mdata_file_add_val_binary_data(mdata_file_t *mdata_file, const char *field, char *data, int data_len);

#endif

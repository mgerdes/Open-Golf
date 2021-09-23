#ifndef _MDATA_H
#define _MDATA_H

#include <stdint.h>

#include "mcore/mfile.h"

struct mdatafile;
typedef struct mdatafile mdatafile_t;

mdatafile_t *mdatafile_load(const char *path);
void mdatafile_delete(mdatafile_t *file);
void mdatafile_save(mdatafile_t *file);
void mdatafile_cache_old_vals(mdatafile_t *file);
void mdatafile_add_int(mdatafile_t *file, const char *name, int val, bool user_set);
void mdatafile_add_string(mdatafile_t *file, const char *name, const char *string, bool user_set);
void mdatafile_add_data(mdatafile_t *file, const char *name, unsigned char *data, int data_len);

#endif

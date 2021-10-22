#ifndef _MCORE_MDATA_H
#define _MCORE_MDATA_H

#include <stdbool.h>

void mdata_init(void);
void mdata_run_import(void);
void mdata_update(float dt);
void mdata_add_loader(const char *ext, bool(*load)(const char *path, char *data, int data_len), bool(*unload)(const char *path), bool(*reload)(const char *path, char *data, int data_len));
void mdata_add_importer(const char *ext, bool(*import)(const char *path, char *data, int data_len));
void mdata_load_file(const char *path);
void mdata_unload_file(const char *path);

#endif


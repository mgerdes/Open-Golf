#ifndef _MIMPORT_H
#define _MIMPORT_H

struct mdatafile;
typedef struct mdatafile mdatafile_t;

mdatafile_t *mdatafile_load(const char *path);
void mdatafile_delete(mdatafile_t *file);
void mdatafile_save(mdatafile_t *file);
void mdatafile_cache_old_vals(mdatafile_t *file);
void mdatafile_add_int(mdatafile_t *file, const char *name, int val, bool user_set);
void mdatafile_add_string(mdatafile_t *file, const char *name, const char *string, bool user_set);
void mdatafile_add_data(mdatafile_t *file, const char *name, unsigned char *data, int data_len);
bool mdatafile_get_string(mdatafile_t *file, const char *name, const char **string);
bool mdatafile_get_data(mdatafile_t *file, const char *name, unsigned char **data, int *data_len);

void mimport_init(void);
void mimport_add_importer(const char *ext, void (*callback)(mdatafile_t *file, void *udata), void *udata);
const char *mdatafile_get_name(mdatafile_t *file);
void mimport_run(bool always_run);

#endif

#ifndef _HOTLOADER_H
#define _HOTLOADER_H

#include <stdbool.h>

#include "mcore/mfile.h"

void hotloader_init(void);
void hotloader_update(void);
void hotloader_watch_file(const char *path, void *udata,
        bool (*callback)(mfile_t file, bool first_time, void *udata));
void hotloader_watch_files(const char *path, void *udata,
        bool (*callback)(mfile_t file, bool first_time, void *udata), bool recurse);
void hotloader_watch_files_with_ext(const char *path, const char *ext, void *udata,
        bool (*callback)(mfile_t file, bool first_time, void *udata), bool recurse);

#endif

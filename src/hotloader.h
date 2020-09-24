#ifndef _HOTLOADER_H
#define _HOTLOADER_H

#include <stdbool.h>

#include "file.h"

void hotloader_init(void);
void hotloader_update(void);
void hotloader_watch_file(const char *path, void *udata,
        bool (*callback)(struct file file, bool first_time, void *udata));
void hotloader_watch_files(const char *path, const char *ext, void *udata,
        bool (*callback)(struct file file, bool first_time, void *udata));

#endif

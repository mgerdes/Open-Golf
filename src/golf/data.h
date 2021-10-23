#ifndef _GOLF_DATA_H
#define _GOLF_DATA_H

#include <stdbool.h>
#include "3rd_party/map/map.h"
#include "3rd_party/sokol/sokol_gfx.h"
#include "golf/string.h"

typedef struct golf_data_script {
    golf_string_t src;
} golf_data_script_t;

typedef struct golf_data_texture {
    int width, height;
    sg_image image;
} golf_data_texture_t;

typedef map_t(golf_data_script_t*) map_golf_data_script_t;

void golf_data_init(void);
void golf_data_run_import(void);
void golf_data_update(float dt);
void golf_data_load_file(const char *path);
void golf_data_unload_file(const char *path);
void golf_data_debug_console_tab(void);

golf_data_script_t *golf_data_get_script(const char *path);

#endif


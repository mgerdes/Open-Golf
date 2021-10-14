#ifndef _GOLF_CONFIG_H
#define _GOLF_CONFIG_H

#include "mcore/maths.h"
#include "mcore/mdata.h"

typedef struct golf_config_file {
    map_mdata_config_property_t props; 
} golf_config_file_t;

typedef map_t(golf_config_file_t) map_golf_config_file_t;

typedef struct golf_config {
    map_golf_config_file_t config_files;
} golf_config_t;

golf_config_t *golf_config_get(void);
void golf_config_init(void);
const char *config_get_string(const char *file, const char *name);
float config_get_number(const char *file, const char *name);
vec2 config_get_vec2(const char *file, const char *name);
vec3 config_get_vec3(const char *file, const char *name);
vec4 config_get_vec4(const char *file, const char *name);

#endif

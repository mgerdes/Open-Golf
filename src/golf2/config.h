#ifndef _GOLF_CONFIG_H
#define _GOLF_CONFIG_H

#include "mcore/maths.h"
#include "mcore/mdata.h"

typedef struct golf_config_file {
    map_mdata_config_property_t props; 
} golf_config_file_t;

typedef map_t(golf_config_file_t) map_golf_config_file_t;

typedef struct golf_config {
    vec_char_ptr_t cur_file_stack;
    map_golf_config_file_t config_files;
} golf_config_t;

#define CFG_STR(name) golf_config_get_string(name)
#define CFG_NUM(name) golf_config_get_number(name)
#define CFG_V2(name) golf_config_get_vec2(name)
#define CFG_V3(name) golf_config_get_vec3(name)
#define CFG_V4(name) golf_config_get_vec4(name)

golf_config_t *golf_config_get(void);
void golf_config_init(void);
void golf_config_push_cur_file(char *file);
void golf_config_pop_cur_file(void);

const char *golf_config_get_string(const char *name);
const float golf_config_get_number(const char *name);
const vec2 golf_config_get_vec2(const char *name);
const vec3 golf_config_get_vec3(const char *name);
const vec4 golf_config_get_vec4(const char *name);

const char *golf_config_get_string_file(const char *file, const char *name);
const char *golf_config_get_string(const char *name);
const float golf_config_get_number(const char *name);
const vec2 golf_config_get_vec2(const char *name);
const vec3 golf_config_get_vec3(const char *name);
const vec4 golf_config_get_vec4(const char *name);
float golf_config_get_number_file(const char *file, const char *name);
vec2 golf_config_get_vec2_file(const char *file, const char *name);
vec3 golf_config_get_vec3_file(const char *file, const char *name);
vec4 golf_config_get_vec4_file(const char *file, const char *name);

#endif

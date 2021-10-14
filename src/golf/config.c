#define _CRT_SECURE_NO_WARNINGS

#include "3rd_party/map/map.h"

#include "mcore/mcommon.h"
#include "mcore/mimport.h"
#include "mcore/mlog.h"

#include "golf/config.h"
#include "golf/log.h"

map_int_t _int_map;
map_float_t _float_map;
map_vec2_t _vec2_map;
map_vec3_t _vec3_map;
map_vec4_t _vec4_map;

static void _import_config(mdatafile_t *file, void *udata) {
    unsigned char *data;
    int data_len;
    if (!mdatafile_get_data(file, "data", &data, &data_len)) {
        mlog_error("Missing data field for config mdatafile");
    }

    char *str = (char*)data;
    int line_buf_len = 1024;
    char *line_buf = malloc(line_buf_len);
    while (mstr_copy_line(&str, &line_buf, &line_buf_len)) {
        char property_name[256];
        property_name[255] = 0;
        int i;
        float f;
        vec2 v2;
        vec3 v3;
        vec4 v4;

        if (sscanf(line_buf, "int %255s = %d", property_name, &i) == 2) {
            map_set(&_int_map, property_name, i);
        }
        else if (sscanf(line_buf, "float %255s = %f", property_name, &f) == 2) {
            map_set(&_float_map, property_name, f);
        }
        else if (sscanf(line_buf, "vec2 %255s = %f,%f", property_name, &v2.x, &v2.y) == 3) {
            map_set(&_vec2_map, property_name, v2);
        }
        else if (sscanf(line_buf, "vec3 %255s = %f,%f,%f", property_name, &v3.x, &v3.y, &v3.z) == 4) {
            map_set(&_vec3_map, property_name, v3);
        }
        else if (sscanf(line_buf, "vec4 %255s = %f,%f,%f,%f", property_name, &v4.x, &v4.y, &v4.z, &v4.w) == 5) {
            map_set(&_vec4_map, property_name, v4);
        }
    }

    free(line_buf);
}

void config_init(void) {
    map_init(&_int_map);
    map_init(&_float_map);
    map_init(&_vec2_map);
    map_init(&_vec3_map);
    mimport_add_importer(".cfg", _import_config, NULL);
}

int config_get_int(const char *name) {
    int *val = map_get(&_int_map, name);
    if (val) {
        return *val;
    }
    else {
        mlog_warning("No config int property %s.", name);
        return 0;
    }
}

float config_get_float(const char *name) {
    float *val = map_get(&_float_map, name);
    if (val) {
        return *val;
    }
    else {
        mlog_warning("No config float property %s.", name);
        return 0.0f;
    }
}

vec2 config_get_vec2(const char *name) {
    vec2 *val = map_get(&_vec2_map, name);
    if (val) {
        return *val;
    }
    else {
        mlog_warning("No config vec2 property %s.", name);
        return V2(0.0f, 0.0f);
    }
}

vec3 config_get_vec3(const char *name) {
    vec3 *val = map_get(&_vec3_map, name);
    if (val) {
        return *val;
    }
    else {
        mlog_warning("No config vec3 property %s.", name);
        return V3(0.0f, 0.0f, 0.0f);
    }
}

vec4 config_get_vec4(const char *name) {
    vec4 *val = map_get(&_vec4_map, name);
    if (val) {
        return *val;
    }
    else {
        mlog_warning("No config vec4 property %s.", name);
        return V4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

#define _CRT_SECURE_NO_WARNINGS


#include "3rd_party/map/map.h"

#include "mcore/mfile.h"

#include "golf/config.h"
#include "golf/hotloader.h"
#include "golf/log.h"

map_int_t _int_map;
map_float_t _float_map;
map_vec2_t _vec2_map;
map_vec3_t _vec3_map;
map_vec4_t _vec4_map;

static bool config_update(mfile_t file, bool first_time, void *udata) {
    if (!mfile_load_data(&file)) {
        m_logf("Could not load file: %s\n", file.path);
        return false;
    }

    char *buf = NULL;
    int buf_len = 0;
    while (mfile_copy_line(&file, &buf, &buf_len)) {
        char property_name[256];
        property_name[255] = 0;
        int i;
        float f;
        vec2 v2;
        vec3 v3;
        vec4 v4;

        if (sscanf(buf, "int %255s = %d", property_name, &i) == 2) {
            map_set(&_int_map, property_name, i);
        }
        else if (sscanf(buf, "float %255s = %f", property_name, &f) == 2) {
            map_set(&_float_map, property_name, f);
        }
        else if (sscanf(buf, "vec2 %255s = %f,%f", property_name, &v2.x, &v2.y) == 3) {
            map_set(&_vec2_map, property_name, v2);
        }
        else if (sscanf(buf, "vec3 %255s = %f,%f,%f", property_name, &v3.x, &v3.y, &v3.z) == 4) {
            map_set(&_vec3_map, property_name, v3);
        }
        else if (sscanf(buf, "vec4 %255s = %f,%f,%f,%f", property_name, &v4.x, &v4.y, &v4.z, &v4.w) == 5) {
            map_set(&_vec4_map, property_name, v4);
        }
    }

    free(buf);
    mfile_free_data(&file);
    return true;
}

void config_init(void) {
    map_init(&_int_map);
    map_init(&_float_map);
    map_init(&_vec2_map);
    map_init(&_vec3_map);
    hotloader_watch_file("assets/minigolf.cfg", NULL, config_update);
}

int config_get_int(const char *name) {
    int *val = map_get(&_int_map, name);
    if (val) {
        return *val;
    }
    else {
        return 0;
    }
}

float config_get_float(const char *name) {
    float *val = map_get(&_float_map, name);
    if (val) {
        return *val;
    }
    else {
        return 0.0f;
    }
}

vec2 config_get_vec2(const char *name) {
    vec2 *val = map_get(&_vec2_map, name);
    if (val) {
        return *val;
    }
    else {
        return V2(0.0f, 0.0f);
    }
}

vec3 config_get_vec3(const char *name) {
    vec3 *val = map_get(&_vec3_map, name);
    if (val) {
        return *val;
    }
    else {
        return V3(0.0f, 0.0f, 0.0f);
    }
}

vec4 config_get_vec4(const char *name) {
    vec4 *val = map_get(&_vec4_map, name);
    if (val) {
        return *val;
    }
    else {
        return V4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

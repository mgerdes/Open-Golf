#include "golf2/config.h"

#include <assert.h>
#include "mcore/mdata.h"
#include "mcore/mlog.h"

static const char *_config_folder = "data/config/";
static golf_config_t _golf_config;

static bool _load_config(const char *path, mdata_t data) {
    assert(data.type == MDATA_CONFIG);

    if (strstr(path, _config_folder) != path) {
        mlog_warning("Config file %s is outside of the %s folder", path, _config_folder);
        return false;
    }

    mdata_config_t *config_data = data.config;
    golf_config_file_t config_file;
    map_init(&config_file.props);
    for (int i = 0; i < config_data->properties.length; i++) {
        mdata_config_property_t prop = config_data->properties.data[i]; 
        map_set(&config_file.props, prop.name, prop);
    }
    map_set(&_golf_config.config_files, path + strlen(_config_folder), config_file);
}

static bool _unload_config(const char *path, mdata_t data) {
    assert(data.type == MDATA_CONFIG);
}

static bool _reload_config(const char *path, mdata_t data) {
    assert(data.type == MDATA_CONFIG);

    golf_config_file_t *config_file = map_get(&_golf_config.config_files, path + strlen(_config_folder));
    if (!config_file) {
        mlog_warning("Attempting to reload config file that isn't loaded %s", path);
        return false;
    }

    map_deinit(&config_file->props);
    _load_config(path, data);
}

golf_config_t *golf_config_get(void) {
    return &_golf_config;
}

void golf_config_init(void) {
    map_init(&_golf_config.config_files);
    mdata_add_loader(MDATA_CONFIG, _load_config, _unload_config, _reload_config);
}

static mdata_config_property_t *_get_config_property(const char *file, const char *name, mdata_config_property_type_t type) {
    golf_config_file_t *config_file = map_get(&_golf_config.config_files, file);
    if (!config_file) {
        return NULL;
    }

    mdata_config_property_t *prop = map_get(&config_file->props, name);
    if (!prop || prop->type != type) {
        return NULL;
    }

    return prop;
}

const char *config_get_string(const char *file, const char *name) {
    mdata_config_property_t *prop = _get_config_property(file, name, MDATA_CONFIG_PROPERTY_STRING);
    if (prop) {
        return prop->string_val;
    }
    else {
        return "";
    }
}

float config_get_number(const char *file, const char *name) {
    mdata_config_property_t *prop = _get_config_property(file, name, MDATA_CONFIG_PROPERTY_NUMBER);
    if (prop) {
        return prop->number_val;
    }
    else {
        return 0.0f;
    }
}

vec2 config_get_vec2(const char *file, const char *name) {
    mdata_config_property_t *prop = _get_config_property(file, name, MDATA_CONFIG_PROPERTY_VEC2);
    if (prop) {
        return prop->vec2_val;
    }
    else {
        return V2(0.0f, 0.0f);
    }
}

vec3 config_get_vec3(const char *file, const char *name) {
    mdata_config_property_t *prop = _get_config_property(file, name, MDATA_CONFIG_PROPERTY_VEC3);
    if (prop) {
        return prop->vec3_val;
    }
    else {
        return V3(0.0f, 0.0f, 0.0f);
    }
}

vec4 config_get_vec4(const char *file, const char *name) {
    mdata_config_property_t *prop = _get_config_property(file, name, MDATA_CONFIG_PROPERTY_VEC4);
    if (prop) {
        return prop->vec4_val;
    }
    else {
        return V4(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

#include "mcore/mdata_texture.h"

#include "golf/parson_helper.h"
#include "golf/string.h"
#include "mcore/mdata.h"

bool mdata_texture_import(const char *path, char *data, int data_len) {
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    json_object_set_string(obj, "filter", "linear");
    json_object_set_data(obj, "data", data, data_len);

    golf_string_t import_texture_file_path;
    golf_string_initf(&import_texture_file_path, "%s.import", path);
    json_serialize_to_file_pretty(val, import_texture_file_path.cstr);
    golf_string_deinit(&import_texture_file_path);
    json_value_free(val);
    return true;
}

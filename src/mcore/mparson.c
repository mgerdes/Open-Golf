#include "mcore/mparson.h"

#include "mcore/mcommon.h"

void json_object_get_data(JSON_Object *obj, const char *name, unsigned char **data, int *data_len) {
    const char *data_base64 = json_object_get_string(obj, name); 
    *data = mbase64_decode(data_base64, strlen(data_base64), data_len);
}

void json_object_set_data(JSON_Object *obj, const char *name, unsigned char *data, int data_len) {
    char *enc = mbase64_encode(data, data_len); 
    json_object_set_string(obj, name, enc);
    free(enc);
}

vec2 json_object_get_vec2(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec2 v;
    v.x = json_array_get_number(array, 0);
    v.y = json_array_get_number(array, 1);
    return v;
}

vec3 json_object_get_vec3(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec3 v;
    v.x = json_array_get_number(array, 0);
    v.y = json_array_get_number(array, 1);
    v.z = json_array_get_number(array, 2);
    return v;
}

vec4 json_object_get_vec4(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec4 v;
    v.x = json_array_get_number(array, 0);
    v.y = json_array_get_number(array, 1);
    v.z = json_array_get_number(array, 2);
    v.w = json_array_get_number(array, 3);
    return v;
}

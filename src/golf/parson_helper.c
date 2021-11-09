#include "golf/parson_helper.h"

vec2 golf_json_object_get_vec2(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec2 v;
    v.x = (float)json_array_get_number(array, 0);
    v.y = (float)json_array_get_number(array, 1);
    return v;
}

vec3 golf_json_object_get_vec3(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec3 v;
    v.x = (float)json_array_get_number(array, 0);
    v.y = (float)json_array_get_number(array, 1);
    v.z = (float)json_array_get_number(array, 2);
    return v;
}

vec4 golf_json_object_get_vec4(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec4 v;
    v.x = (float)json_array_get_number(array, 0);
    v.y = (float)json_array_get_number(array, 1);
    v.z = (float)json_array_get_number(array, 2);
    v.w = (float)json_array_get_number(array, 3);
    return v;
}

void golf_json_object_set_vec2(JSON_Object *obj, const char *name, vec2 v) {
    JSON_Value *val = json_value_init_array();
    JSON_Array *arr = json_value_get_array(val);
    json_array_append_number(arr, v.x);
    json_array_append_number(arr, v.y);
    json_object_set_value(obj, name, val);
}

void golf_json_object_set_vec3(JSON_Object *obj, const char *name, vec3 v) {
    JSON_Value *val = json_value_init_array();
    JSON_Array *arr = json_value_get_array(val);
    json_array_append_number(arr, v.x);
    json_array_append_number(arr, v.y);
    json_array_append_number(arr, v.z);
    json_object_set_value(obj, name, val);
}

void golf_json_object_set_vec4(JSON_Object *obj, const char *name, vec4 v) {
    JSON_Value *val = json_value_init_array();
    JSON_Array *arr = json_value_get_array(val);
    json_array_append_number(arr, v.x);
    json_array_append_number(arr, v.y);
    json_array_append_number(arr, v.z);
    json_array_append_number(arr, v.w);
    json_object_set_value(obj, name, val);
}

void golf_json_object_set_quat(JSON_Object *obj, const char *name, quat q) {
    JSON_Value *val = json_value_init_array();
    JSON_Array *arr = json_value_get_array(val);
    json_array_append_number(arr, q.x);
    json_array_append_number(arr, q.y);
    json_array_append_number(arr, q.z);
    json_array_append_number(arr, q.w);
    json_object_set_value(obj, name, val);
}

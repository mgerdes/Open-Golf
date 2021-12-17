#include "golf/json.h"

#include "golf/alloc.h"
#include "golf/base64.h"
#include "golf/log.h"

void *_parson_malloc(size_t size) {
    return golf_alloc_tracked(size, "parson");
}

void _parson_free(void *mem) {
    golf_free(mem);
}

void golf_json_init(void) {
    json_set_allocation_functions(_parson_malloc, _parson_free);
}

void golf_json_object_get_data(JSON_Object *obj, const char *name, unsigned char **data, int *data_len) {
    const char *enc_data = json_object_get_string(obj, name); 
    int enc_len = (int)strlen(enc_data);
    *data = golf_base64_decode((const unsigned char*)enc_data, enc_len, data_len);
    if (!data) {
        golf_log_warning("Failed to decode data in field %s", name);
    }
}

void golf_json_object_set_data(JSON_Object *obj, const char *name, unsigned char *data, int data_len) {
    int enc_len;
    unsigned char *enc_data = golf_base64_encode(data, data_len, &enc_len);
    if (!enc_data) {
        golf_log_warning("Failed to encode data in field %s", name);
    }
    json_object_set_string(obj, name, (char*)enc_data);
    golf_free(enc_data);
}

void golf_json_array_get_data(JSON_Array *arr, int idx, unsigned char **data, int *data_len) {
    const char *enc_data = json_array_get_string(arr, idx);
    int enc_len = (int)strlen(enc_data);
    *data = golf_base64_decode((const unsigned char*)enc_data, enc_len, data_len);
    if (!data) {
        golf_log_warning("Failed to decode data in array");
    }
}

void golf_json_array_append_data(JSON_Array *arr, unsigned char *data, int data_len) {
    int enc_len;
    unsigned char *enc_data = golf_base64_encode(data, data_len, &enc_len);
    if (!enc_data) {
        golf_log_warning("Failed to encode data in array");
    }
    json_array_append_string(arr, (char*)enc_data);
    golf_free(enc_data);
}

vec2 golf_json_object_get_vec2(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec2 v;
    v.x = (float)json_array_get_number(array, 0);
    v.y = (float)json_array_get_number(array, 1);
    return v;
}

void golf_json_object_set_vec2(JSON_Object *obj, const char *name, vec2 v) {
    JSON_Value *val = json_value_init_array();
    JSON_Array *arr = json_value_get_array(val);
    json_array_append_number(arr, v.x);
    json_array_append_number(arr, v.y);
    json_object_set_value(obj, name, val);
}

vec3 golf_json_object_get_vec3(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    vec3 v;
    v.x = (float)json_array_get_number(array, 0);
    v.y = (float)json_array_get_number(array, 1);
    v.z = (float)json_array_get_number(array, 2);
    return v;
}

void golf_json_object_set_vec3(JSON_Object *obj, const char *name, vec3 v) {
    JSON_Value *val = json_value_init_array();
    JSON_Array *arr = json_value_get_array(val);
    json_array_append_number(arr, v.x);
    json_array_append_number(arr, v.y);
    json_array_append_number(arr, v.z);
    json_object_set_value(obj, name, val);
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

void golf_json_object_set_vec4(JSON_Object *obj, const char *name, vec4 v) {
    JSON_Value *val = json_value_init_array();
    JSON_Array *arr = json_value_get_array(val);
    json_array_append_number(arr, v.x);
    json_array_append_number(arr, v.y);
    json_array_append_number(arr, v.z);
    json_array_append_number(arr, v.w);
    json_object_set_value(obj, name, val);
}

quat golf_json_object_get_quat(JSON_Object *obj, const char *name) {
    JSON_Array *array = json_object_get_array(obj, name);
    quat q;
    q.x = (float)json_array_get_number(array, 0);
    q.y = (float)json_array_get_number(array, 1);
    q.z = (float)json_array_get_number(array, 2);
    q.w = (float)json_array_get_number(array, 3);
    return q;
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

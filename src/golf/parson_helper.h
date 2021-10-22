#ifndef _GOLF_PARSON_HELPER_H
#define _GOLF_PARSON_HELPER_H

#include "3rd_party/parson/parson.h"
#include "golf/maths.h"

void json_object_get_data(JSON_Object *obj, const char *name, unsigned char **data, int *data_len);
void json_object_set_data(JSON_Object *obj, const char *name, unsigned char *data, int data_len);

vec2 json_object_get_vec2(JSON_Object *obj, const char *name);
vec3 json_object_get_vec3(JSON_Object *obj, const char *name);
vec4 json_object_get_vec4(JSON_Object *obj, const char *name);

#endif

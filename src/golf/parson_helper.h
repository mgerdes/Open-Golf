#ifndef _GOLF_PARSON_HELPER_H
#define _GOLF_PARSON_HELPER_H

#include "parson/parson.h"
#include "golf/maths.h"

vec2 golf_json_object_get_vec2(JSON_Object *obj, const char *name);
vec3 golf_json_object_get_vec3(JSON_Object *obj, const char *name);
vec4 golf_json_object_get_vec4(JSON_Object *obj, const char *name);
quat golf_json_object_get_quat(JSON_Object *obj, const char *name);
void golf_json_object_set_vec2(JSON_Object *obj, const char *name, vec2 v);
void golf_json_object_set_vec3(JSON_Object *obj, const char *name, vec3 v);
void golf_json_object_set_vec4(JSON_Object *obj, const char *name, vec4 v);
void golf_json_object_set_quat(JSON_Object *obj, const char *name, quat q);

#endif

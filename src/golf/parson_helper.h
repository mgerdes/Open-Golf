#ifndef _GOLF_PARSON_HELPER_H
#define _GOLF_PARSON_HELPER_H

#include "parson/parson.h"
#include "golf/level.h"
#include "golf/maths.h"

void golf_json_object_get_data(JSON_Object *obj, const char *name, unsigned char **data, int *data_len);
void golf_json_object_set_data(JSON_Object *obj, const char *name, unsigned char *data, int data_len);

vec2 golf_json_object_get_vec2(JSON_Object *obj, const char *name);
void golf_json_object_set_vec2(JSON_Object *obj, const char *name, vec2 v);

vec3 golf_json_object_get_vec3(JSON_Object *obj, const char *name);
void golf_json_object_set_vec3(JSON_Object *obj, const char *name, vec3 v);

vec4 golf_json_object_get_vec4(JSON_Object *obj, const char *name);
void golf_json_object_set_vec4(JSON_Object *obj, const char *name, vec4 v);

quat golf_json_object_get_quat(JSON_Object *obj, const char *name);
void golf_json_object_set_quat(JSON_Object *obj, const char *name, quat q);

void golf_json_object_get_transform(JSON_Object *obj, const char *name, golf_transform_t *transform);
void golf_json_object_set_transform(JSON_Object *obj, const char *name, golf_transform_t *transform);

void golf_json_object_get_lightmap(JSON_Object *obj, const char *name, golf_lightmap_t *lightmap);
void golf_json_object_set_lightmap(JSON_Object *obj, const char *name, golf_lightmap_t *lightmap);

#endif

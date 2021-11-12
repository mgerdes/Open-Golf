#ifndef _GOLF_LEVEL_H
#define _GOLF_LEVEL_H

#include "vec/vec.h"
#include "golf/data.h"
#include "golf/file.h"
#include "golf/maths.h"

#define GOLF_MATERIAL_NAME_MAX_LEN 64
typedef struct golf_material {
    char name[GOLF_MATERIAL_NAME_MAX_LEN];
    char texture_path[GOLF_FILE_MAX_PATH];
    golf_texture_t *texture;
    float friction;
    float restitution;
} golf_material_t;
typedef vec_t(golf_material_t) vec_golf_material_t;

typedef struct golf_transform {
    vec3 position;
    quat rotation;
    vec3 scale;
} golf_transform_t;
mat4 golf_transform_get_model_mat(golf_transform_t transform);

typedef struct golf_ball_start_entity {
    golf_transform_t transform;
} golf_ball_start_entity_t;

typedef struct golf_model_entity {
    golf_transform_t transform;
    char model_path[GOLF_FILE_MAX_PATH];
    golf_model_t *model;
} golf_model_entity_t;
typedef vec_t(golf_model_entity_t) vec_golf_model_entity_t;

typedef enum golf_entity_type {
    MODEL_ENTITY,
    BALL_START_ENTITY,
} golf_entity_type_t;

typedef struct golf_entity {
    bool active;
    golf_entity_type_t type;
    union {
        golf_model_entity_t model;
        golf_ball_start_entity_t ball_start;
    };
} golf_entity_t;
typedef vec_t(golf_entity_t) vec_golf_entity_t;

typedef struct golf_level {
    vec_golf_material_t materials;
    vec_golf_entity_t entities;
} golf_level_t;

bool golf_level_save(golf_level_t *level, const char *path);
bool golf_level_load(golf_level_t *level, const char *path, char *data, int data_len);
bool golf_level_unload(golf_level_t *level);
bool golf_level_get_material(golf_level_t *level, const char *material_name, golf_material_t *material);

golf_transform_t *golf_entity_get_transform(golf_entity_t *entity);

#endif

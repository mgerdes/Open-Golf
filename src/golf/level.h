#ifndef _GOLF_LEVEL_H
#define _GOLF_LEVEL_H

#include "vec/vec.h"
#include "golf/data.h"
#include "golf/file.h"
#include "golf/maths.h"

#define GOLF_MAX_NAME_LEN 64

typedef struct golf_lightmap_image {
    bool active;
    char name[GOLF_MAX_NAME_LEN];
    int resolution;
    unsigned char *data;
    int width, height;
    sg_image sg_image;
} golf_lightmap_image_t;
typedef vec_t(golf_lightmap_image_t) vec_golf_lightmap_image_t;
void golf_lightmap_image_init(golf_lightmap_image_t *lightmap, const char *name, int resolution, int width, int height, unsigned char *data);

typedef struct golf_lightmap_section {
    char lightmap_name[GOLF_MAX_NAME_LEN];
    vec_vec2_t uvs;
    sg_buffer sg_uvs_buf;
} golf_lightmap_section_t;
void golf_lightmap_section_init(golf_lightmap_section_t *section, const char *lightmap_name, vec_vec2_t uvs, int start, int count);

typedef struct golf_lightmap {
    int resolution;
    unsigned char *image_data;
    int image_width, image_height;
    sg_image sg_image;
    vec_vec2_t uvs;
    sg_buffer sg_uvs_buf;
} golf_lightmap_t;
void golf_lightmap_init(golf_lightmap_t *lightmap, int resolution, int image_width, int image_height, unsigned char *image_data, vec_vec2_t uvs);

typedef enum golf_material_type {
    GOLF_MATERIAL_TEXTURE,
    GOLF_MATERIAL_COLOR,
    GOLF_MATERIAL_DIFFUSE_COLOR,
    GOLF_MATERIAL_ENVIRONMENT,
} golf_material_type_t;

typedef struct golf_material {
    bool active;
    char name[GOLF_MAX_NAME_LEN];
    float friction;
    float restitution;

    golf_material_type_t type; 
    union {
        struct {
            char texture_path[GOLF_FILE_MAX_PATH];
            golf_texture_t *texture;
        };
        vec3 color;
    };
} golf_material_t;
typedef vec_t(golf_material_t) vec_golf_material_t;
golf_material_t golf_material_color(vec3 color);
golf_material_t golf_material_texture(const char *texture_path);

typedef struct golf_transform {
    vec3 position;
    quat rotation;
    vec3 scale;
} golf_transform_t;
golf_transform_t golf_transform(vec3 position, vec3 scale, quat rotation);
mat4 golf_transform_get_model_mat(golf_transform_t transform);

typedef struct golf_ball_start_entity {
    golf_transform_t transform;
} golf_ball_start_entity_t;

typedef struct golf_model_entity {
    golf_transform_t transform;

    int lightmap_idx;
    vec_vec2_t lightmap_uvs;
    sg_buffer sg_lightmap_uvs_buf;

    golf_lightmap_section_t lightmap_section;
    golf_lightmap_t lightmap;
    char model_path[GOLF_FILE_MAX_PATH];
    golf_model_t *model;
} golf_model_entity_t;
typedef vec_t(golf_model_entity_t) vec_golf_model_entity_t;

typedef struct golf_hole_entity {
    golf_transform_t transform;
} golf_hole_entity_t;

typedef enum golf_entity_type {
    MODEL_ENTITY,
    BALL_START_ENTITY,
    HOLE_ENTITY,
} golf_entity_type_t;

typedef struct golf_entity {
    bool active;
    golf_entity_type_t type;
    union {
        golf_model_entity_t model;
        golf_ball_start_entity_t ball_start;
        golf_hole_entity_t hole;
    };
} golf_entity_t;
typedef vec_t(golf_entity_t) vec_golf_entity_t;
golf_entity_t golf_entity_model(golf_transform_t transform, const char *model_path, golf_lightmap_t lightmap, golf_lightmap_section_t lightmap_section);
golf_entity_t golf_entity_hole(golf_transform_t transform);
golf_entity_t golf_entity_ball_start(golf_transform_t transform);
golf_entity_t golf_entity_make_copy(golf_entity_t *entity);
golf_transform_t *golf_entity_get_transform(golf_entity_t *entity);
golf_lightmap_t *golf_entity_get_lightmap(golf_entity_t *entity);
golf_lightmap_section_t *golf_entity_get_lightmap_section(golf_entity_t *entity);
golf_model_t *golf_entity_get_model(golf_entity_t *entity);

typedef struct golf_level {
    vec_golf_lightmap_image_t lightmap_images;
    vec_golf_material_t materials;
    vec_golf_entity_t entities;
} golf_level_t;
bool golf_level_save(golf_level_t *level, const char *path);
bool golf_level_load(golf_level_t *level, const char *path, char *data, int data_len);
bool golf_level_unload(golf_level_t *level);
bool golf_level_get_material(golf_level_t *level, const char *material_name, golf_material_t *out_material);
bool golf_level_get_lightmap_image(golf_level_t *level, const char *lightmap_name, golf_lightmap_image_t *out_lightmap_image);

#endif

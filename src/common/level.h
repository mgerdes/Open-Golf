#ifndef _GOLF_LEVEL_H
#define _GOLF_LEVEL_H

#include "common/data.h"
#include "common/file.h"
#include "common/maths.h"
#include "common/script.h"
#include "common/string.h"
#include "common/vec.h"

typedef struct golf_geo_generator_data_arg {
    char name[GOLF_MAX_NAME_LEN];
    gs_val_t val;
} golf_geo_generator_data_arg_t;
typedef vec_t(golf_geo_generator_data_arg_t) vec_golf_geo_generator_data_arg_t;

typedef struct golf_geo_generator_data {
    golf_script_t *script;
    vec_golf_geo_generator_data_arg_t args;
} golf_geo_generator_data_t;
golf_geo_generator_data_t golf_geo_generator_data(golf_script_t *script, vec_golf_geo_generator_data_arg_t args);
bool golf_geo_generator_data_get_arg(golf_geo_generator_data_t *data, const char *name, golf_geo_generator_data_arg_t **arg);

typedef enum golf_geo_face_uv_gen_type {
    GOLF_GEO_FACE_UV_GEN_MANUAL,
    GOLF_GEO_FACE_UV_GEN_GROUND,
    GOLF_GEO_FACE_UV_GEN_WALL_SIDE,
    GOLF_GEO_FACE_UV_GEN_WALL_TOP,
    GOLF_GEO_FACE_UV_GEN_COUNT,
} golf_geo_face_uv_gen_type_t;
const char **golf_geo_uv_gen_type_strings(void);

typedef struct golf_geo_face {
    bool active;
    char material_name[GOLF_MAX_NAME_LEN];
    vec_int_t idx;
    golf_geo_face_uv_gen_type_t uv_gen_type;
    vec_vec2_t uvs;
    int start_vertex_in_model;
    vec3 water_dir;
} golf_geo_face_t;
typedef vec_t(golf_geo_face_t) vec_golf_geo_face_t;
typedef vec_t(golf_geo_face_t*) vec_golf_geo_face_ptr_t;
golf_geo_face_t golf_geo_face(const char *material_name, vec_int_t idx, golf_geo_face_uv_gen_type_t uv_gen_type, vec_vec2_t uvs, vec3 water_dir);

typedef struct golf_geo_point {
    bool active;
    vec3 position;
} golf_geo_point_t;
typedef vec_t(golf_geo_point_t) vec_golf_geo_point_t;
golf_geo_point_t golf_geo_point(vec3 position);

typedef struct golf_geo {
    golf_geo_generator_data_t generator_data;
    vec_golf_geo_point_t points;
    vec_golf_geo_face_t faces;
    bool is_water;
    bool model_updated_this_frame;
    golf_model_t model;
} golf_geo_t;
golf_geo_t golf_geo(vec_golf_geo_point_t points, vec_golf_geo_face_t faces, golf_geo_generator_data_t generator_data, bool is_water);
void golf_geo_finalize(golf_geo_t *geo);
void golf_geo_update_model(golf_geo_t *geo);

typedef enum golf_movement_type {
    GOLF_MOVEMENT_NONE,
    GOLF_MOVEMENT_LINEAR,
    GOLF_MOVEMENT_SPINNER,
    GOLF_MOVEMENT_PENDULUM,
} golf_movement_type_t;

typedef struct golf_movement {
    golf_movement_type_t type;  
    bool repeats;
    float t0, length;
    union {
        struct {
            vec3 p0, p1;
        } linear;
        struct {
            float theta0;
            vec3 axis;
        } pendulum;
    };
} golf_movement_t;
golf_movement_t golf_movement_none(void);
golf_movement_t golf_movement_linear(float t0, vec3 p0, vec3 p1, float length);
golf_movement_t golf_movement_spinner(float t0, float length);
golf_movement_t golf_movement_pendulum(float t0, float length, float theta0, vec3 axis);

typedef struct golf_lightmap_image {
    bool active;
    char name[GOLF_MAX_NAME_LEN];
    int resolution;
    int width, height;
    float time_length;
    bool repeats;
    int num_samples, edited_num_samples;
    unsigned char **data;
    sg_image *sg_image;
} golf_lightmap_image_t;
typedef vec_t(golf_lightmap_image_t) vec_golf_lightmap_image_t;
golf_lightmap_image_t golf_lightmap_image(const char *name, int resolution, int width, int height, float time_length, bool repeats, int num_samples, unsigned char **data, sg_image *sg_image);
void golf_lightmap_image_finalize(golf_lightmap_image_t *lightmap);

typedef struct golf_lightmap_section {
    char lightmap_name[GOLF_MAX_NAME_LEN];
    vec_vec2_t uvs;
    sg_buffer sg_uvs_buf;
} golf_lightmap_section_t;
golf_lightmap_section_t golf_lightmap_section(const char *lightmap_name, vec_vec2_t uvs);
void golf_lightmap_section_finalize(golf_lightmap_section_t *section);

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
    float vel_scale;

    golf_material_type_t type; 
    union {
        struct {
            char texture_path[GOLF_FILE_MAX_PATH];
            golf_texture_t *texture;
        };
        vec4 color;
    };
} golf_material_t;
typedef vec_t(golf_material_t) vec_golf_material_t;
golf_material_t golf_material_texture(const char *name, float friction, float restitution, float vel_scale, const char *texture_path);
golf_material_t golf_material_diffuse_color(const char *name, float friction, float restitution, float vel_scale, vec4 color);
golf_material_t golf_material_color(const char *name, float friction, float resitution, float vel_scale, vec4 color);
golf_material_t golf_material_environment(const char *name, float friction, float resitution, float vel_scale, const char *texture_path);

typedef struct golf_transform {
    vec3 position;
    quat rotation;
    vec3 scale;
} golf_transform_t;
typedef vec_t(golf_transform_t) vec_golf_transform_t;
golf_transform_t golf_transform(vec3 position, vec3 scale, quat rotation);
mat4 golf_transform_get_model_mat(golf_transform_t transform);
golf_transform_t golf_transform_apply_movement(golf_transform_t transform, golf_movement_t movement, float t);
golf_transform_t golf_transform_apply_transform(golf_transform_t transform, golf_transform_t movement);

typedef struct golf_ball_start_entity {
    golf_transform_t transform;
} golf_ball_start_entity_t;

typedef struct golf_model_entity {
    golf_transform_t transform;
    golf_lightmap_section_t lightmap_section;
    golf_movement_t movement;
    char model_path[GOLF_FILE_MAX_PATH];
    golf_model_t *model;
    float uv_scale;
} golf_model_entity_t;
typedef vec_t(golf_model_entity_t) vec_golf_model_entity_t;

typedef struct golf_hole_entity {
    golf_transform_t transform;
} golf_hole_entity_t;

typedef struct golf_geo_entity {
    golf_lightmap_section_t lightmap_section;
    golf_transform_t transform;
    golf_movement_t movement;
    golf_geo_t geo;
} golf_geo_entity_t;

typedef struct golf_water_entity {
    golf_lightmap_section_t lightmap_section;
    golf_transform_t transform;
    golf_geo_t geo;
} golf_water_entity_t;

typedef struct golf_entity golf_entity_t;
typedef vec_t(golf_entity_t) vec_golf_entity_t;
typedef struct golf_group_entity {
    golf_transform_t transform;
} golf_group_entity_t;

typedef struct golf_begin_animation_entity {
    golf_transform_t transform;
} golf_begin_animation_entity_t;

typedef struct golf_camera_zone_entity {
    golf_transform_t transform;
} golf_camera_zone_entity_t;

typedef enum golf_entity_type {
    MODEL_ENTITY,
    BALL_START_ENTITY,
    HOLE_ENTITY,
    GEO_ENTITY,
    WATER_ENTITY,
    GROUP_ENTITY,
    BEGIN_ANIMATION_ENTITY,
    CAMERA_ZONE_ENTITY,
} golf_entity_type_t;

typedef struct golf_entity {
    bool active;
    int parent_idx;
    golf_entity_type_t type;
    char name[GOLF_MAX_NAME_LEN];
    union {
        golf_model_entity_t model;
        golf_ball_start_entity_t ball_start;
        golf_hole_entity_t hole;
        golf_geo_entity_t geo;
        golf_water_entity_t water;
        golf_group_entity_t group;
        golf_begin_animation_entity_t begin_animation;
        golf_camera_zone_entity_t camera_zone;
    };
} golf_entity_t;
golf_entity_t golf_entity_model(const char *name, golf_transform_t transform, const char *model_path, float uv_scale, golf_lightmap_section_t lightmap_section, golf_movement_t movement);
golf_entity_t golf_entity_hole(const char *name, golf_transform_t transform);
golf_entity_t golf_entity_ball_start(const char *name, golf_transform_t transform);
golf_entity_t golf_entity_geo(const char *name, golf_transform_t transform, golf_movement_t movement, golf_geo_t geo, golf_lightmap_section_t lightmap_section);
golf_entity_t golf_entity_water(const char *name, golf_transform_t transform, golf_geo_t geo, golf_lightmap_section_t lightmap_section);
golf_entity_t golf_entity_group(const char *name, golf_transform_t transform);
golf_entity_t golf_entity_begin_animation(const char *name, golf_transform_t transform);
golf_entity_t golf_entity_camera_zone(const char *name, golf_transform_t transform);
golf_entity_t golf_entity_make_copy(golf_entity_t *entity);
golf_movement_t *golf_entity_get_movement(golf_entity_t *entity);
golf_transform_t *golf_entity_get_transform(golf_entity_t *entity);
golf_transform_t golf_entity_get_world_transform(golf_level_t *level, golf_entity_t *entity);
golf_lightmap_section_t *golf_entity_get_lightmap_section(golf_entity_t *entity);
golf_model_t *golf_entity_get_model(golf_entity_t *entity);
golf_geo_t *golf_entity_get_geo(golf_entity_t *entity);
vec3 golf_entity_get_velocity(golf_level_t *level, golf_entity_t *entity, float t, vec3 world_point);
bool golf_level_get_camera_zone_direction(golf_level_t *level, vec3 pos, vec3 *dir);

typedef struct golf_level {
    vec_golf_file_t deps; 
    vec_golf_lightmap_image_t lightmap_images;
    vec_golf_material_t materials;
    vec_golf_entity_t entities;
} golf_level_t;
bool golf_level_save(golf_level_t *level, const char *path);
bool golf_level_get_material(golf_level_t *level, const char *material_name, golf_material_t *out_material);
bool golf_level_get_lightmap_image(golf_level_t *level, const char *lightmap_name, golf_lightmap_image_t *out_lightmap_image);

#endif

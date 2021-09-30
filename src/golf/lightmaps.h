#if defined(_WIN32) && 0
#ifndef _LIGHTMAPS_H
#define _LIGHTMAPS_H

#include "array.h"
#include "hole.h"

struct lightmap_entity {
    mat4 model_mat;
    struct array_vec3 positions, normals;
    struct array_vec2 lightmap_uvs;
    int lightmap_width, lightmap_height;
    float *lightmap_data;
    struct lightmap *lightmap;

    int gl_position_vbo, gl_lightmap_uv_vbo, gl_tex; 
};
array_t(struct lightmap_entity, array_lightmap_entity)

struct lightmap_multi_entity {
    struct array_mat4 moving_model_mats;
    struct array_vec3 moving_positions, moving_normals;
    struct array_vec2 moving_lightmap_uvs;
    int moving_lightmap_width, moving_lightmap_height;
    struct array_float_ptr moving_lightmap_data;
    struct lightmap *moving_lightmap;

    mat4 static_model_mat;
    struct array_vec3 static_positions, static_normals;
    struct array_vec2 static_lightmap_uvs;
    int static_lightmap_width, static_lightmap_height;
    struct array_float_ptr static_lightmap_data;
    struct lightmap *static_lightmap;

    struct array_int gl_moving_tex, gl_static_tex;
    int gl_moving_position_vbo, gl_moving_lightmap_uv_vbo;
    int gl_static_position_vbo, gl_static_lightmap_uv_vbo;
};
array_t(struct lightmap_multi_entity, array_lightmap_multi_entity)

struct lightmap_generator_data {
    bool reset_lightmaps, create_uvs;
    float gamma;
    int num_iterations, num_dilates, num_smooths;
    struct array_lightmap_entity entities;
    struct array_lightmap_multi_entity multi_entities;
};

void lightmap_generator_data_init(struct hole *hole, struct lightmap_generator_data *data, 
        bool reset_lightmaps, bool create_uvs, float gamma,
        int num_iterations, int num_smooths, int num_dilates);
void lightmap_generator_data_deinit(struct lightmap_generator_data *data);
void lightmap_generator_init(void);
void lightmap_generator_start(struct lightmap_generator_data *data);
bool lightmap_generator_is_running(void);
void lightmap_generator_get_progress(int *idx, float *pct);
void lightmap_generator_get_progress_string(char *buffer, int buffer_len);

#endif
#endif

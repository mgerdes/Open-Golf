#ifndef _GOLF_LIGHTMAP_H
#define _GOLF_LIGHTMAP_H

#include "mattiasgustavsson_libs/thread.h"
#include "vec/vec.h"
#include "golf/level.h"
#include "golf/maths.h"

typedef struct golf_lightmap_entity {
    mat4 model_mat;
    vec_vec3_t positions, normals;
    vec_vec2_t lightmap_uvs;
    int lightmap_size;
    float *lightmap_data;
    int gl_position_vbo, gl_lightmap_uv_vbo, gl_tex;
} golf_lightmap_entity_t;
typedef vec_t(golf_lightmap_entity_t) vec_golf_lightmap_entity_t;

typedef struct golf_lightmap_generator {
    bool reset_lightmaps, create_uvs;
    float gamma;
    int num_iterations, num_dilates, num_smooths;
    vec_golf_lightmap_entity_t entities;

    thread_ptr_t thread;
    thread_mutex_t lock;
    bool is_running;
    int uv_gen_progress, lm_gen_progress, lm_gen_progress_pct;
} golf_lightmap_generator_t;

void golf_lightmap_generator_init(golf_lightmap_generator_t *generator, bool reset_lightmaps, bool create_uvs, float gamma, int num_iterations, int num_dilates, int num_smooths);
void golf_lightmap_generator_deinit(golf_lightmap_generator_t *generator);
void golf_lightmap_generator_add_entity(golf_lightmap_generator_t *generator, golf_model_t *model, mat4 model_mat, vec_vec2_t lightmap_uvs, int lightmap_size, float *lightmap_data);
int golf_lightmap_generator_get_lm_gen_progress(golf_lightmap_generator_t *generator);
int golf_lightmap_generator_get_uv_gen_progress(golf_lightmap_generator_t *generator);
bool golf_lightmap_generator_is_running(golf_lightmap_generator_t *generator);
void golf_lightmap_generator_start(golf_lightmap_generator_t *generator);

#endif

#ifndef _GOLF_LIGHTMAP_H
#define _GOLF_LIGHTMAP_H

#include "mattiasgustavsson_libs/thread.h"
#include "vec/vec.h"
#include "golf/level.h"
#include "golf/maths.h"

typedef struct golf_gi_lightmap_section {
    int start, count;
    int gl_position_vbo, gl_lightmap_uv_vbo;

    golf_lightmap_section_t *lightmap_section;
} golf_gi_lightmap_section_t;
typedef vec_t(golf_gi_lightmap_section_t) vec_golf_gi_lightmap_section_t;

typedef struct golf_gi_entity {
    vec_vec3_t positions, normals;
    vec_vec2_t lightmap_uvs;
    int resolution, image_width, image_height;
    float *image_data;
    int gl_position_vbo, gl_lightmap_uv_vbo, gl_tex;

    golf_lightmap_image_t *lightmap_image;
    vec_golf_gi_lightmap_section_t gi_lightmap_sections;
} golf_gi_entity_t;
typedef vec_t(golf_gi_entity_t) vec_golf_gi_entity_t;

typedef struct golf_gi {
    bool reset_lightmaps, create_uvs;

    float gamma, z_near, z_far, interpolation_threshold, camera_to_surface_distance_modifier;
    int num_iterations, num_dilates, num_smooths, hemisphere_size, interpolation_passes;

    bool has_cur_entity;
    golf_gi_entity_t cur_entity;
    vec_golf_gi_entity_t entities;

    thread_ptr_t thread;
    thread_mutex_t lock;
    bool is_running;
    int uv_gen_progress, lm_gen_progress, lm_gen_progress_pct;
} golf_gi_t;

void golf_gi_init(golf_gi_t *generator,
        bool reset_lightmaps, bool create_uvs, float gamma, 
        int num_iterations, int num_dilates, int num_smooths,
        int hemisphere_size, float z_near, float z_far,
        int interpolation_passes, float interpolation_threshold,
        float camera_to_surface_distance_modifier);
void golf_gi_deinit(golf_gi_t *generator);
void golf_gi_start_lightmap(golf_gi_t *gi, golf_lightmap_image_t *lightmap_image);
void golf_gi_end_lightmap(golf_gi_t *gi);
void golf_gi_add_lightmap_section(golf_gi_t *gi, golf_lightmap_section_t *lightmap_section, golf_model_t *model, mat4 model_mat);
int golf_gi_get_lm_gen_progress(golf_gi_t *generator);
int golf_gi_get_uv_gen_progress(golf_gi_t *generator);
bool golf_gi_is_running(golf_gi_t *generator);
void golf_gi_start(golf_gi_t *generator);

#endif

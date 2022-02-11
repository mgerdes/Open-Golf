#ifndef _GOLF_BVH_H
#define _GOLF_BVH_H

#include "common/data.h"
#include "common/level.h"
#include "common/maths.h"
#include "common/vec.h"

typedef struct golf_bvh_aabb {
    vec3 min, max;
} golf_bvh_aabb_t;

typedef struct golf_bvh_node_info {
    int idx;
    golf_model_t *model;
    golf_transform_t transform;
} golf_bvh_node_info_t;
typedef vec_t(golf_bvh_node_info_t) vec_golf_bvh_node_info_t;
golf_bvh_node_info_t golf_bvh_node_info(int idx, golf_model_t *model, golf_transform_t transform);

typedef struct golf_bvh_node golf_bvh_node_t;
typedef struct golf_bvh_node {
    bool is_leaf;
    golf_bvh_node_info_t leaf_node_info;

    golf_bvh_aabb_t aabb;
    int left, right;
} golf_bvh_node_t;
typedef vec_t(golf_bvh_node_t) vec_golf_bvh_node_t; 

typedef struct golf_bvh {
    vec_golf_bvh_node_info_t node_infos;
    vec_golf_bvh_node_t nodes;
    int parent;
} golf_bvh_t;

typedef struct golf_ball_contact {
    bool is_ignored;
    vec3 position, normal, velocity;
    float distance, penetration, resitution, friction, impulse_mag, vel_scale, cull_dot;
} golf_ball_contact_t;

void golf_bvh_init(golf_bvh_t *bvh);
void golf_bvh_construct(golf_bvh_t *bvh, vec_golf_bvh_node_info_t);
bool golf_bvh_ray_test(golf_bvh_t *bvh, vec3 ro, vec3 rd, float *t, int *idx);
bool golf_bvh_ball_test(golf_bvh_t *bvh, vec3 ball_pos, float ball_radius, vec3 ball_velocity, golf_ball_contact_t *contacts, int *num_ball_contacts, int max_ball_contacts);

#endif

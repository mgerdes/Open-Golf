#ifndef _GOLF_BVH_H
#define _GOLF_BVH_H

#include "golf/data.h"
#include "golf/level.h"
#include "golf/maths.h"
#include "golf/vec.h"

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
    vec_golf_bvh_node_t nodes;
    int parent;
} golf_bvh_t;

void golf_bvh_init(golf_bvh_t *bvh);
void golf_bvh_construct(golf_bvh_t *bvh, vec_golf_bvh_node_info_t);
bool golf_bvh_ray_test(golf_bvh_t *bvh, vec3 ro, vec3 rd, float *t, int *idx);

#endif

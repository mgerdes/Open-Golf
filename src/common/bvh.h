#ifndef _GOLF_BVH_H
#define _GOLF_BVH_H

#include "common/data.h"
#include "common/level.h"
#include "common/maths.h"
#include "common/vec.h"

typedef struct golf_bvh golf_bvh_t;

typedef struct golf_bvh_face {
    float t;
    golf_level_t *level;
    golf_entity_t *entity;
    vec3 a, b, c;
    float restitution, friction, vel_scale;
} golf_bvh_face_t;
typedef vec_t(golf_bvh_face_t) vec_golf_bvh_face_t;

typedef struct golf_bvh_aabb {
    vec3 min, max;
} golf_bvh_aabb_t;

typedef struct golf_bvh_node_info {
    int idx;
    golf_bvh_aabb_t aabb;
    vec3 pos;
    int face_start, face_count;
} golf_bvh_node_info_t;
typedef vec_t(golf_bvh_node_info_t) vec_golf_bvh_node_info_t;
golf_bvh_node_info_t golf_bvh_node_info(golf_bvh_t *bvh, int idx, golf_level_t *level, golf_entity_t *entity, float t);

typedef struct golf_bvh_node golf_bvh_node_t;
typedef struct golf_bvh_node {
    bool is_leaf;
    golf_bvh_node_info_t leaf_node_info;

    golf_bvh_aabb_t aabb;
    int left, right;
} golf_bvh_node_t;
typedef vec_t(golf_bvh_node_t) vec_golf_bvh_node_t; 

typedef struct golf_bvh {
    vec_golf_bvh_face_t faces;
    vec_golf_bvh_node_info_t node_infos;
    vec_golf_bvh_node_t nodes;
    int parent;
} golf_bvh_t;

typedef struct golf_ball_contact {
    bool is_ignored;
    triangle_contact_type_t type;
    vec3 position, normal, velocity, v0, v1, triangle_normal, impulse;
    float distance, penetration, impulse_mag, cull_dot;
    vec3 triangle_a, triangle_b, triangle_c;
    float restitution, friction, vel_scale;
} golf_ball_contact_t;
typedef vec_t(golf_ball_contact_t) vec_golf_ball_contact_t;

golf_ball_contact_t golf_ball_contact(vec3 a, vec3 b, vec3 c, vec3 vel, vec3 bp, float br, vec3 cp, float dist, float restitution, float friction, float vel_scale, triangle_contact_type_t type);

void golf_bvh_init(golf_bvh_t *bvh);
void golf_bvh_construct(golf_bvh_t *bvh, vec_golf_bvh_node_info_t);
bool golf_bvh_ray_test(golf_bvh_t *bvh, vec3 ro, vec3 rd, float *t, int *idx, golf_bvh_face_t *face);
bool golf_bvh_ball_test(golf_bvh_t *bvh, vec3 ball_pos, float ball_radius, vec3 ball_velocity, golf_ball_contact_t *contacts, int *num_ball_contacts, int max_ball_contacts);

#endif

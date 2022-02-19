#include "common/bvh.h"

#include <assert.h>
#include <float.h>

#include "common/log.h"

static void _update_aabb(golf_bvh_aabb_t *aabb, vec3 p) {
    if (p.x < aabb->min.x) aabb->min.x = p.x;
    if (p.y < aabb->min.y) aabb->min.y = p.y;
    if (p.z < aabb->min.z) aabb->min.z = p.z;
    if (p.x > aabb->max.x) aabb->max.x = p.x;
    if (p.y > aabb->max.y) aabb->max.y = p.y;
    if (p.z > aabb->max.z) aabb->max.z = p.z;
}

golf_bvh_node_info_t golf_bvh_node_info(golf_bvh_t *bvh, int idx, golf_level_t *level, golf_entity_t *entity, float t) {
    golf_model_t *model = golf_entity_get_model(entity);
    golf_transform_t transform = golf_entity_get_world_transform(level, entity);
    golf_movement_t *movement = golf_entity_get_movement(entity);

    golf_bvh_node_info_t info;
    info.idx = idx;
    info.face_start = bvh->faces.length;
    info.face_count = model->positions.length / 3;
    info.pos = V3(0, 0, 0);

    golf_transform_t moved_transform = transform;
    if (movement) {
        moved_transform = golf_transform_apply_movement(moved_transform, *movement, t);
    }
    mat4 model_mat = golf_transform_get_model_mat(moved_transform);

    for (int i = 0; i < model->groups.length; i++) {
        golf_model_group_t group = model->groups.data[i];

        golf_material_t material;
        if (!golf_level_get_material(level, group.material_name, &material)) {
            material = golf_material_texture("", 0, 0, 0, "data/textures/fallback.png");
        }

        for (int j = 0; j < group.vertex_count; j += 3) {
            int v_idx = group.start_vertex + j;
            vec3 a = vec3_apply_mat4(model->positions.data[v_idx + 0], 1, model_mat);
            vec3 b = vec3_apply_mat4(model->positions.data[v_idx + 1], 1, model_mat);
            vec3 c = vec3_apply_mat4(model->positions.data[v_idx + 2], 1, model_mat);

            info.pos = vec3_add(info.pos, a);
            info.pos = vec3_add(info.pos, b);
            info.pos = vec3_add(info.pos, c);

            golf_bvh_face_t face;
            face.a = a;
            face.b = b;
            face.c = c;
            face.restitution = material.restitution;
            face.friction = material.friction;
            face.vel_scale = material.vel_scale;
            face.level = level;
            face.entity = entity;
            face.t = t;
            vec_push(&bvh->faces, face);

            if (i == 0) {
                info.aabb.min = a;
                info.aabb.max = a;
            }

            _update_aabb(&info.aabb, a);
            _update_aabb(&info.aabb, b);
            _update_aabb(&info.aabb, c);
        }
    }

    info.pos = vec3_scale(info.pos, 1.0f / model->positions.length);

    return info;
}

static golf_bvh_aabb_t _aabb_combine(golf_bvh_aabb_t a, golf_bvh_aabb_t b) {
    golf_bvh_aabb_t c;
    c.min = V3(fminf(a.min.x, b.min.x), fminf(a.min.y, b.min.y), fminf(a.min.z, b.min.z));
    c.max = V3(fmaxf(a.max.x, b.max.x), fmaxf(a.max.y, b.max.y), fmaxf(a.max.z, b.max.z));
    return c;
}

static int _alloc_node(golf_bvh_t *bvh) {
    golf_bvh_node_t node;
    memset(&node, 0, sizeof(node));
    vec_push(&bvh->nodes, node);
    return bvh->nodes.length - 1;
}

static golf_bvh_node_t *_get_node(golf_bvh_t *bvh, int idx) {
    return &bvh->nodes.data[idx];
}

static int _alloc_leaf_node(golf_bvh_t *bvh, golf_bvh_node_info_t info) {
    int idx = _alloc_node(bvh);
    golf_bvh_node_t *node = _get_node(bvh, idx);
    node->is_leaf = true;
    node->leaf_node_info = info;
    node->aabb = info.aabb;
    node->left = -1;
    node->right = -1;
    return idx;
}

static int _alloc_inner_node(golf_bvh_t *bvh, int left, int right) {
    int idx = _alloc_node(bvh);
    golf_bvh_node_t *node = _get_node(bvh, idx);
    node->is_leaf = false;
    node->aabb = _aabb_combine(_get_node(bvh, left)->aabb, _get_node(bvh, right)->aabb);
    node->left = left;
    node->right = right;
    return idx;
}

golf_ball_contact_t golf_ball_contact(vec3 a, vec3 b, vec3 c, vec3 vel, vec3 bp, float br, vec3 cp, float dist, float restitution, float friction, float vel_scale, triangle_contact_type_t type)  {
    golf_ball_contact_t contact;
    contact.is_ignored = false;
    contact.position = cp;
    contact.triangle_normal = vec3_normalize(vec3_cross(vec3_sub(b, a), vec3_sub(c, a)));
    switch (type) {
        case TRIANGLE_CONTACT_FACE:
            contact.normal = contact.triangle_normal;
            break;
        case TRIANGLE_CONTACT_AB:
        case TRIANGLE_CONTACT_AC:
        case TRIANGLE_CONTACT_BC:
        case TRIANGLE_CONTACT_A:
        case TRIANGLE_CONTACT_B:
        case TRIANGLE_CONTACT_C:
            contact.normal = vec3_normalize(vec3_sub(bp, cp));
            break;
    }
    contact.velocity = vel;
    contact.triangle_a = a;
    contact.triangle_b = b;
    contact.triangle_c = c;
    contact.restitution = restitution;
    contact.friction = friction;
    contact.vel_scale = vel_scale;
    contact.type = type;
    contact.penetration = br - dist;
    return contact;
}

void golf_bvh_init(golf_bvh_t *bvh) {
    vec_init(&bvh->faces, "bvh");
    vec_init(&bvh->node_infos, "bvh");
    vec_init(&bvh->nodes, "bvh");
    bvh->parent = -1;
}

static vec3 _cmp_axis;

static int _bvh_node_cmp(const void *a, const void *b) {
    golf_bvh_node_info_t *node_a = (golf_bvh_node_info_t*)a;
    golf_bvh_node_info_t *node_b = (golf_bvh_node_info_t*)b;
    float dot_a = vec3_dot(_cmp_axis, node_a->pos);
    float dot_b = vec3_dot(_cmp_axis, node_b->pos);
    if (dot_a > dot_b) {
        return 1;
    }
    else if (dot_a == dot_b) {
        return 0;
    }
    else {
        return -1;
    }
}

static int _golf_bvh_construct(golf_bvh_t *bvh, vec_golf_bvh_node_info_t node_infos, int idx, int n) {
    if (n == 0) {
        return -1;
    }
    else if (n == 1) {
        return _alloc_leaf_node(bvh, node_infos.data[idx]);
    }
    else if (n == 2) {
        int left = _alloc_leaf_node(bvh, node_infos.data[idx]);
        int right = _alloc_leaf_node(bvh, node_infos.data[idx + 1]);
        return _alloc_inner_node(bvh, left, right);
    }
    else {
        _cmp_axis = vec3_normalize(V3(golf_randf(-1, 1), golf_randf(-1, 1), golf_randf(-1, 1)));
        qsort(node_infos.data + idx, n, sizeof(golf_bvh_node_info_t), _bvh_node_cmp);

        int left_n = n / 2;
        int right_n = n - left_n;

        int left = _golf_bvh_construct(bvh, node_infos, idx, left_n);
        int right = _golf_bvh_construct(bvh, node_infos, idx + left_n, right_n);
        return _alloc_inner_node(bvh, left, right);
    }
}

void golf_bvh_construct(golf_bvh_t *bvh, vec_golf_bvh_node_info_t node_infos) {
    bvh->faces.length = 0;
    bvh->nodes.length = 0;
    bvh->parent = _golf_bvh_construct(bvh, node_infos, 0, node_infos.length);
}

static bool _golf_bvh_ray_test(golf_bvh_t *bvh, int node_idx, vec3 ro, vec3 rd, float *t, int *idx, golf_bvh_face_t *face) {
    if (node_idx < 0) {
        return false;
    }

    golf_bvh_node_t *node = _get_node(bvh, node_idx);

    float t0;
    if (!ray_intersect_aabb(ro, rd, node->aabb.min, node->aabb.max, &t0)) {
        return false;
    }

    if (node->is_leaf) {
        golf_bvh_node_info_t info = node->leaf_node_info;

        *t = FLT_MAX;
        *idx = -1;

        for (int i = 0; i < info.face_count; i++) {
            int face_idx = info.face_start + i; 
            golf_bvh_face_t hit_face = bvh->faces.data[face_idx];
            vec3 triangle_points[3] = { hit_face.a, hit_face.b, hit_face.c };

            float t0;
            int idx0;
            if (ray_intersect_triangles(ro, rd, triangle_points, 3, &t0, &idx0)) {
                if (t0 < *t) {
                    if (face) {
                        *face = hit_face;
                    }
                    *t = t0;
                    *idx = info.idx;
                }
            }
        }

        return *t < FLT_MAX;
    }
    else {
        float t_left;
        int idx_left;
        golf_bvh_face_t left_face;
        bool left_test = _golf_bvh_ray_test(bvh, node->left, ro, rd, &t_left, &idx_left, &left_face);

        float t_right;
        int idx_right;
        golf_bvh_face_t right_face;
        bool right_test = _golf_bvh_ray_test(bvh, node->right, ro, rd, &t_right, &idx_right, &right_face);

        if (left_test && right_test) {
            if (t_left < t_right) {
                *t = t_left;
                *idx = idx_left;
                if (face) *face = left_face;
            }
            else {
                *t = t_right;
                *idx = idx_right;
                if (face) *face = right_face;
            }
            return true;
        }
        else if (left_test && !right_test) {
            *t = t_left;
            *idx = idx_left;
            if (face) *face = left_face;
            return true;
        }
        else if (!left_test && right_test) {
            *t = t_right;
            *idx = idx_right;
            if (face) *face = right_face;
            return true;
        }
        else {
            return false;
        }
    }
}

bool golf_bvh_ray_test(golf_bvh_t *bvh, vec3 ro, vec3 rd, float *t, int *idx, golf_bvh_face_t *face) {
    return _golf_bvh_ray_test(bvh, bvh->parent, ro, rd, t, idx, face);    
}

static bool _golf_bvh_ball_test(golf_bvh_t *bvh, int node_idx, vec3 bp, float br, vec3 bv, golf_ball_contact_t *contacts, int *num_ball_contacts, int max_ball_contacts) {
    if (node_idx < 0) {
        return false;
    }

    golf_bvh_node_t *node = _get_node(bvh, node_idx);
    if (!sphere_intersect_aabb(bp, br, node->aabb.min, node->aabb.max)) {
        return false;
    }

    if (node->is_leaf) {
        golf_bvh_node_info_t info = node->leaf_node_info;

        bool any_hit = false;
        for (int i = 0; i < info.face_count; i++) {
            int face_idx = info.face_start + i;
            golf_bvh_face_t face = bvh->faces.data[face_idx];

            triangle_contact_type_t type;
            vec3 cp = closest_point_point_triangle(bp, face.a, face.b, face.c, &type);
            float dist = vec3_distance(bp, cp);
            if (dist < br) {
                if (*num_ball_contacts < max_ball_contacts) {
                    vec3 vel = golf_entity_get_velocity(face.level, face.entity, face.t, cp);
                    golf_ball_contact_t contact = golf_ball_contact(face.a, face.b, face.c, vel, bp, br, cp, dist, face.restitution, face.friction, face.vel_scale, type);
                    contacts[*num_ball_contacts] = contact;
                    *num_ball_contacts = *num_ball_contacts + 1;
                }

                any_hit = true;
            }
        }
        return any_hit;
    }
    else {
        bool left_test = _golf_bvh_ball_test(bvh, node->left, bp, br, bv, contacts, num_ball_contacts, max_ball_contacts);
        bool right_test = _golf_bvh_ball_test(bvh, node->right, bp, br, bv, contacts, num_ball_contacts, max_ball_contacts);
        return left_test || right_test;
    }

    return true;
}

bool golf_bvh_ball_test(golf_bvh_t *bvh, vec3 bp, float br, vec3 bv, golf_ball_contact_t *contacts, int *num_ball_contacts, int max_ball_contacts) {
    return _golf_bvh_ball_test(bvh, bvh->parent, bp, br, bv, contacts, num_ball_contacts, max_ball_contacts);
}

#include "common/bvh.h"

#include <assert.h>

golf_bvh_node_info_t golf_bvh_node_info(int idx, golf_model_t *model, golf_transform_t transform) {
    golf_bvh_node_info_t info;
    info.idx = idx;
    info.model = model;
    info.transform = transform;
    return info;
}

static golf_bvh_aabb_t _aabb_combine(golf_bvh_aabb_t a, golf_bvh_aabb_t b) {
    golf_bvh_aabb_t c;
    c.min = V3(fminf(a.min.x, b.min.x), fminf(a.min.y, b.min.y), fminf(a.min.z, b.min.z));
    c.max = V3(fmaxf(a.max.x, b.max.x), fmaxf(a.max.y, b.max.y), fmaxf(a.max.z, b.max.z));
    return c;
}

static golf_bvh_aabb_t _leaf_node_aabb(golf_bvh_node_info_t info) {
    golf_bvh_aabb_t a;

    golf_model_t *model = info.model;
    mat4 model_mat = golf_transform_get_model_mat(info.transform);
    for (int i = 0; i < model->positions.length; i++) {
        vec3 p = model->positions.data[i];
        p = vec3_apply_mat4(p, 1, model_mat);

        if (i == 0) {
            a.min = p;
            a.max = p;
        }
        else {
            if (p.x < a.min.x) a.min.x = p.x;
            if (p.y < a.min.y) a.min.y = p.y;
            if (p.z < a.min.z) a.min.z = p.z;
            if (p.x > a.max.x) a.max.x = p.x;
            if (p.y > a.max.y) a.max.y = p.y;
            if (p.z > a.max.z) a.max.z = p.z;
        }
    }

    return a;
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
    node->aabb = _leaf_node_aabb(info);
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

void golf_bvh_init(golf_bvh_t *bvh) {
    vec_init(&bvh->nodes, "bvh");
    bvh->parent = -1;
}

static vec3 _cmp_axis;

static int _bvh_node_cmp(const void *a, const void *b) {
    golf_bvh_node_info_t *node_a = (golf_bvh_node_info_t*)a;
    golf_bvh_node_info_t *node_b = (golf_bvh_node_info_t*)b;
    float dot_a = vec3_dot(_cmp_axis, node_a->transform.position);
    float dot_b = vec3_dot(_cmp_axis, node_b->transform.position);
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
    bvh->nodes.length = 0;
    bvh->parent = _golf_bvh_construct(bvh, node_infos, 0, node_infos.length);
}

static bool _golf_bvh_ray_test(golf_bvh_t *bvh, int node_idx, vec3 ro, vec3 rd, float *t, int *idx) {
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
        golf_model_t *model = info.model;
        mat4 model_mat = golf_transform_get_model_mat(info.transform);

        float triangle_t;
        int triangle_idx;
        if (ray_intersect_triangles_with_transform(ro, rd, model->positions.data, model->positions.length, model_mat, &triangle_t, &triangle_idx)) {
            *t = triangle_t;
            *idx = info.idx;
            return true;
        }
        else {
            return false;
        }
    }
    else {
        float t_left;
        int idx_left;
        bool left_test = _golf_bvh_ray_test(bvh, node->left, ro, rd, &t_left, &idx_left);

        float t_right;
        int idx_right;
        bool right_test = _golf_bvh_ray_test(bvh, node->right, ro, rd, &t_right, &idx_right);

        if (left_test && right_test) {
            if (t_left < t_right) {
                *t = t_left;
                *idx = idx_left;
            }
            else {
                *t = t_right;
                *idx = idx_right;
            }
            return true;
        }
        else if (left_test && !right_test) {
            *t = t_left;
            *idx = idx_left;
            return true;
        }
        else if (!left_test && right_test) {
            *t = t_right;
            *idx = idx_right;
            return true;
        }
        else {
            return false;
        }
    }
}

bool golf_bvh_ray_test(golf_bvh_t *bvh, vec3 ro, vec3 rd, float *t, int *idx) {
    return _golf_bvh_ray_test(bvh, bvh->parent, ro, rd, t, idx);    
}

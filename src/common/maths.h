#ifndef _MATHS_H
#define _MATHS_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/map.h"
#include "common/vec.h"

#define MF_PI ((float) M_PI)
#define DEGREES_PER_RADIAN 0.01745329251f
#define EPSILON 0.00001f

int golf_clampi(int v, int min, int max);
float golf_clampf(float v, float min, float max);
float golf_snapf(float v, float s);
float golf_randf(float min, float max);

typedef struct vec2 {
    float x, y;
} vec2;
typedef vec_t(vec2) vec_vec2_t;
typedef map_t(vec2) map_vec2_t;

typedef struct vec3 {
    float x, y, z;
} vec3;
typedef vec_t(vec3) vec_vec3_t;
typedef map_t(vec3) map_vec3_t;

typedef struct vec4 {
    float x, y, z, w;
} vec4;
typedef vec_t(vec4) vec_vec4_t;
typedef map_t(vec4) map_vec4_t;

typedef struct line_segment_2d {
    vec2 p0, p1;
} line_segment_2d;

/*
 * 4 x 4 matrices
 * Stored in row-order form
 */
typedef struct mat4 {
    float m[16];
} mat4;

typedef struct quat {
    float x, y, z, w;
} quat;
typedef vec_t(quat) vec_quat_t;

struct ray_2D {
    vec2 orig;
    vec2 dir;
};

struct ray {
    vec3 orig;
    vec3 dir;
};

struct bounding_box {
    vec3 center;
    vec3 half_lengths;
};

struct rect_2D {
    vec2 top_left;
    vec2 size;
};

struct ray_2D ray_2D_create(vec2 origin, vec2 direction);
vec2 ray_2D_at_time(struct ray_2D r, float t);
bool ray_2D_intersect(struct ray_2D r1, struct ray_2D r2, float *t1, float *t2);

struct ray ray_create(vec3 origin, vec3 direction);
struct bounding_box bounding_box_create(vec3 center, vec3 half_lengths);

#define V2 vec2_create 
#define V3 vec3_create
#define V4 vec4_create
#define QUAT quat_create

#define V2_ZERO V2(0.0f, 0.0f)
#define V3_ZERO V3(0.0f, 0.0f, 0.0f)
#define V4_ZERO V4(0.0f, 0.0f, 0.0f, 0.0f)

vec2 vec2_create(float x, float y);
vec2 vec2_create_from_array(float *a);
vec2 vec2_sub(vec2 v1, vec2 v2);
vec2 vec2_normalize(vec2 v);
vec2 vec2_scale(vec2 v, float s);
vec2 vec2_div(vec2 v, float s);
vec2 vec2_interpolate(vec2 p0, vec2 p1, float a);
float vec2_determinant(vec2 u, vec2 v);
float vec2_length(vec2 v);
float vec2_distance_squared(vec2 v1, vec2 v2);
float vec2_distance(vec2 u, vec2 v);
vec2 vec2_add_scaled(vec2 v1, vec2 v2, float s);
vec2 vec2_add(vec2 v1, vec2 v2);
float vec2_length_squared(vec2 v);
float vec2_dot(vec2 v1, vec2 v2);
float vec2_cross(vec2 u, vec2 v);
vec2 vec2_reflect(vec2 u, vec2 v);
vec2 vec2_parallel_component(vec2 u, vec2 v);
vec2 vec2_rotate(vec2 v, float theta);
vec2 vec2_perpindicular_component(vec2 u, vec2 v);
vec2 vec2_set_length(vec2 v, float l);
vec2 vec2_isometric_projection(vec2 v, float scale, float angle);
bool vec2_point_left_of_line(vec2 point, vec2 line_p0, vec2 line_p1);
bool vec2_point_right_of_line(vec2 point, vec2 line_p0, vec2 line_p1);
bool vec2_lines_intersect(vec2 line0_p0, vec2 line0_p1, vec2 line1_p0, vec2 line1_p1);
bool vec2_point_on_line(vec2 point, vec2 line_p0, vec2 line_p1);
bool vec2_point_in_polygon(vec2 point, int num_poly_points, vec2 *poly_points);
vec2 vec2_bezier(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float t);
bool vec2_equal(vec2 v1, vec2 v2);
void vec2_print(vec2 v);

vec3 vec3_create(float x, float y, float z);
vec3 vec3_create_from_array(float *a);
vec3 vec3_add(vec3 v1, vec3 v2);
vec3 vec3_sub(vec3 v1, vec3 v2);
vec3 vec3_add_scaled(vec3 v1, vec3 v2, float s);
vec3 vec3_subtract(vec3 v1, vec3 v2);
vec3 vec3_normalize(vec3 v);
float vec3_dot(vec3 v1, vec3 v2);
vec3 vec3_cross(vec3 v1, vec3 v2);
vec3 vec3_apply_quat(vec3 v, float w, quat q);
vec3 vec3_apply_mat4(vec3 v, float w, mat4 m);
float vec3_distance(vec3 v1, vec3 v2);
float vec3_distance_squared(vec3 v1, vec3 v2);
vec3 vec3_set_length(vec3 v, float l);
float vec3_length(vec3 v);
float vec3_length_squared(vec3 v);
vec3 vec3_scale(vec3 v, float s);
vec3 vec3_div(vec3 v, float s);
bool vec3_equal(vec3 v1, vec3 v2);
vec3 vec3_rotate_x(vec3 v, float theta);
vec3 vec3_rotate_y(vec3 v, float theta);
vec3 vec3_rotate_z(vec3 v, float theta);
vec3 vec3_rotate_about_axis(vec3 vec, vec3 axis, float theta);
vec3 vec3_reflect(vec3 u, vec3 v);
vec3 vec3_reflect_with_restitution(vec3 u, vec3 v, float e);
vec3 vec3_parallel_component(vec3 u, vec3 v);
vec3 vec3_perpindicular_component(vec3 u, vec3 v);
vec3 vec3_interpolate(vec3 v1, vec3 v2, float t);
vec3 vec3_from_hex_color(int hex_color);
vec3 vec3_multiply(vec3 v1, vec3 v2);
vec3 vec3_orthogonal(vec3 u);
vec3 vec3_snap(vec3 v, float s);
float vec3_distance_squared_point_line_segment(vec3 p, vec3 a, vec3 b);
bool vec3_point_on_line_segment(vec3 p, vec3 a, vec3 b, float eps);
bool vec3_line_segments_on_same_line(vec3 a_p0, vec3 a_p1, vec3 b_p0, vec3 b_p1, float eps); 
void vec3_print(vec3 v);

vec4 vec4_create(float x, float y, float z, float w);
vec4 vec4_add(vec4 a, vec4 b);
vec4 vec4_sub(vec4 a, vec4 b);
vec4 vec4_apply_mat(vec4 v, mat4 m);
vec4 vec4_scale(vec4 v, float s);
void vec4_normalize_this(vec4 v);
void vec4_print(vec4 v);

mat4 mat4_create(float a, float b, float c, float d,
        float e, float f, float g, float h,
        float i, float j, float k, float l,
        float m, float n, float o, float p);
bool mat4_equal(mat4 m0, mat4 m1);
mat4 mat4_zero(void);
mat4 mat4_identity(void);
mat4 mat4_translation(vec3 v);
mat4 mat4_scale(vec3 v);
mat4 mat4_rotation_x(float theta);
mat4 mat4_rotation_y(float theta);
mat4 mat4_rotation_z(float theta);
mat4 mat4_shear(float theta, vec3 a, vec3 b);
mat4 mat4_multiply_n(int count, ...);
mat4 mat4_multiply(mat4 m1, mat4 m2);
mat4 mat4_inverse(mat4 m);
mat4 mat4_transpose(mat4 m);
mat4 mat4_normal_transform(mat4 m);
mat4 mat4_look_at(vec3 position, vec3 target, vec3 up);
mat4 mat4_from_axes(vec3 x, vec3 y, vec3 z);
void mat4_get_axes(mat4 m, vec3 *x, vec3 *y, vec3 *z);
mat4 mat4_perspective_projection(float fov, float aspect, float near, float far);
mat4 mat4_orthographic_projection(float left, float right, float bottom, float top, float near, float far);
mat4 mat4_from_quat(quat q);
mat4 mat4_box_inertia_tensor(vec3 half_lengths, float mass);
mat4 mat4_sphere_inertia_tensor(float radius, float mass);
mat4 mat4_triangle_transform(vec2 src_p1, vec2 src_p2, vec2 src_p3,
        vec2 dest_p1, vec2 dest_p2, vec2 dest_p3);
mat4 mat4_box_to_line_transform(vec3 p0, vec3 p1, float sz);
mat4 mat4_interpolate(mat4 m0, mat4 m1, float t);
void mat4_print(mat4 m);

quat quat_create(float x, float y, float z, float w);
float quat_dot(quat v0, quat v1);
quat quat_add(quat v0, quat v1);
quat quat_scale(quat v, float s);
quat quat_subtract(quat v0, quat v1);
quat quat_create_from_axis_angle(vec3 axis, float angle);
quat quat_multiply(quat u, quat v);
quat quat_interpolate(quat u, quat v, float a);
quat quat_slerp(quat u, quat v, float a);
quat quat_normalize(quat q);
quat quat_between_vectors(vec3 u, vec3 v);
void quat_get_axes(quat q, vec3 *x, vec3 *y, vec3 *z);
void quat_get_axis_angle(quat q, vec3 *axis, float *angle);
void quat_print(quat q);

struct rect_2D rect_2D_create(vec2 tl, vec2 sz);
bool rect_2D_contains_point(struct rect_2D rect, vec2 pt);

bool point_inside_box(vec3 p, vec3 box_center, vec3 box_half_lengths);

typedef enum triangle_contact_type {
    TRIANGLE_CONTACT_A,
    TRIANGLE_CONTACT_B,
    TRIANGLE_CONTACT_C,
    TRIANGLE_CONTACT_AB,
    TRIANGLE_CONTACT_AC,
    TRIANGLE_CONTACT_BC,
    TRIANGLE_CONTACT_FACE,
} triangle_contact_type_t;

void ray_intersect_triangles_all(vec3 ro, vec3 rd, vec3 *points, int num_points, mat4 transform, float *t);
bool ray_intersect_triangles_with_transform(vec3 ro, vec3 rd, vec3 *points, int num_points, mat4 transform, float *t, int *idx);
bool ray_intersect_triangles(vec3 ro, vec3 rd, vec3 *points, int num_points, float *t, int *idx);
bool ray_intersect_spheres(vec3 ro, vec3 rd, vec3 *center, float *radius, int num_spheres, float *t, int *idx);
bool ray_intersect_segments(vec3 ro, vec3 rd, vec3 *p0, vec3 *p1, float *radius, int num_segments, float *t, int *idx);
bool ray_intersect_planes(vec3 ro, vec3 rd, vec3 *p, vec3 *n, int num_planes, float *t, int *idx);
bool ray_intersect_aabb(vec3 ro, vec3 rd, vec3 aabb_min, vec3 aabb_max, float *t);
bool sphere_intersect_aabb(vec3 sp, float sr, vec3 aabb_min, vec3 aabb_max);
bool sphere_intersect_triangles_with_transform(vec3 sp, float sr, vec3 *points, int num_points, mat4 transform, triangle_contact_type_t *type, bool *hit);
void triangles_inside_box(vec3 *triangle_points, int num_triangles, vec3 box_center, vec3 box_half_lengths,
        bool *is_inside);
void triangles_inside_frustum(vec3 *triangle_points, int num_triangles, vec3 *frustum_corners, bool *is_inside);

vec3 closest_point_point_plane(vec3 point, vec3 plane_point, vec3 plane_normal);
vec3 closest_point_point_circle(vec3 point, vec3 circle_center, vec3 circle_plane, float circle_radius);

vec3 closest_point_point_triangle(vec3 p, vec3 a, vec3 b, vec3 c, enum triangle_contact_type *type);
vec3 closest_point_point_obb(vec3 p, vec3 bc, vec3 bx, vec3 by, vec3 bz, float bex, float bey, float bez);
float closest_point_ray_segment(vec3 p1, vec3 q1, vec3 p2, vec3 q2, float *s, float *t, vec3 *c1, vec3 *c2);
float closest_point_ray_ray(vec3 p1, vec3 q1, vec3 p2, vec3 q2, float *s, float *t, vec3 *c1, vec3 *c2);
float closest_point_ray_circle(vec3 ro, vec3 rd, vec3 cc, vec3 cn, float cr, float *s, vec3 *c1, vec3 *c2);

bool intersection_moving_sphere_triangle(vec3 sc, float sr, vec3 v, vec3 tp0, vec3 tp1, vec3 tp2, float *t,
        vec3 *p, vec3 *n);

#endif

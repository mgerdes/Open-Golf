#include "maths.h"

#include <float.h>

struct ray ray_create(vec3 orig, vec3 dir) {
	struct ray ray;
	ray.orig = orig;
	ray.dir = dir;
	return ray;
}

struct ray_2D ray_2D_create(vec2 orig, vec2 dir) {
	struct ray_2D ray_2D;
	ray_2D.orig = orig;
	ray_2D.dir = dir;
	return ray_2D;
}

vec2 ray_2D_at_time(struct ray_2D r, float t) {
    return vec2_add(r.orig, vec2_scale(r.dir, t));
}

struct bounding_box bounding_box_create(vec3 center, vec3 half_lengths) {
	struct bounding_box bounding_box;
	bounding_box.center = center;
	bounding_box.half_lengths = half_lengths;
	return bounding_box;
}

bool ray_2D_intersect(struct ray_2D r1, struct ray_2D r2, float *t1, float *t2) {
    mat4 m = mat4_create(r1.dir.x, -r2.dir.x, 0.0f, 0.0f,
                         r1.dir.y, -r2.dir.y, 0.0f, 0.0f,
                         0.0f, 0.0f, 1.0f, 0.0f,
                         0.0f, 0.0f, 0.0f, 1.0f);
    mat4 mi = mat4_inverse(m);
    vec4 v = vec4_create(r2.orig.x - r1.orig.x, r2.orig.y - r1.orig.y, 0.0f, 0.0f);
    vec4 t_v = vec4_apply_mat(v, mi);
    *t1 = t_v.x;
    *t2 = t_v.y;
    return true;
}

vec2 vec2_create(float x, float y) {
	vec2 v;
	v.x = x;
	v.y = y;
	return v;
}

vec2 vec2_sub(vec2 v1, vec2 v2) {
    float x = v1.x - v2.x;
    float y = v1.y - v2.y;
    return vec2_create(x, y);
}

float vec2_length(vec2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

vec2 vec2_normalize(vec2 v) {
    float l = sqrtf(v.x * v.x + v.y * v.y);
    if (l == 0.0f) {
        return v;
    }
    return vec2_create(v.x / l, v.y / l);
}

vec2 vec2_scale(vec2 v, float s) {
    return vec2_create(v.x * s, v.y * s);
}

vec2 vec2_div(vec2 v, float s) {
    return vec2_create(v.x / s, v.y / s);
}

vec2 vec2_interpolate(vec2 p0, vec2 p1, float a) {
    float x = p0.x + (p1.x - p0.x) * a;
    float y = p0.y + (p1.y - p0.y) * a;
    return vec2_create(x, y);
}

float vec2_distance_squared(vec2 v1, vec2 v2) {
    float dx = v1.x - v2.x;
    float dy = v1.y - v2.y;
    return dx * dx + dy * dy;
}

vec2 vec2_add_scaled(vec2 v1, vec2 v2, float s) {
    float x = v1.x + v2.x * s;
    float y = v1.y + v2.y * s;
    return vec2_create(x, y);
}

vec2 vec2_add(vec2 v1, vec2 v2) {
    float x = v1.x + v2.x;
    float y = v1.y + v2.y;
    return vec2_create(x, y);
}

float vec2_dot(vec2 v1, vec2 v2) {
    return v1.x * v2.x + v1.y * v2.y;
}

float vec2_cross(vec2 u, vec2 v) {
    return u.y * v.x - u.x * v.y;
}

float vec2_length_squared(vec2 v) {
    return v.x * v.x + v.y * v.y;
}

vec2 vec2_reflect(vec2 u, vec2 v) {
    return vec2_sub(vec2_perpindicular_component(u, v), vec2_parallel_component(u, v));
}

vec2 vec2_parallel_component(vec2 u, vec2 v) {
    float len_sqrd_v = vec2_length_squared(v);
    if (len_sqrd_v == 0.0f) {
        return V2(0.0f, 0.0f);
    }
    float l = vec2_dot(u, v) / len_sqrd_v;
    return vec2_scale(v, l);
}

vec2 vec2_perpindicular_component(vec2 u, vec2 v) {
    return vec2_sub(u, vec2_parallel_component(u, v));
}

float vec2_determinant(vec2 u, vec2 v) {
    return u.x * v.y - u.y * v.x;
}

float vec2_distance(vec2 u, vec2 v) {
    float dx = u.x - v.x;
    float dy = u.y - v.y;
    return sqrtf(dx * dx + dy * dy);
}

vec2 vec2_rotate(vec2 v, float theta) {
    vec2 u;
    u.x = v.x * cosf(theta) - v.y * sinf(theta);
    u.y = v.y * cosf(theta) + v.x * sinf(theta);
    return u;
}

vec2 vec2_set_length(vec2 v, float l) {
    return vec2_scale(vec2_normalize(v), l);
}

vec2 vec2_isometric_projection(vec2 v, float scale, float angle) {
    vec2 vp;

    vp = vec2_rotate(v, angle);
    vp.y *= scale;

    return vp;
}

bool vec2_point_left_of_line(vec2 point, vec2 line_p0, vec2 line_p1) {
    vec2 a = line_p0;
    vec2 b = line_p1;
    vec2 c = point;
    float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    return area > 0.0f;
}

bool vec2_point_right_of_line(vec2 point, vec2 line_p0, vec2 line_p1) {
    vec2 a = line_p0;
    vec2 b = line_p1;
    vec2 c = point;
    float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    return area < 0.0f;
}

bool vec2_point_on_line(vec2 point, vec2 line_p0, vec2 line_p1) {
    vec2 a = line_p0;
    vec2 b = line_p1;
    vec2 c = point;
    float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    return area == 0.0f;
}

bool vec2_lines_intersect(vec2 line0_p0, vec2 line0_p1, vec2 line1_p0, vec2 line1_p1) {
    bool left1 = vec2_point_left_of_line(line0_p0, line1_p0, line1_p1);
    bool left2 = vec2_point_left_of_line(line0_p1, line1_p0, line1_p1);
    bool left3 = vec2_point_left_of_line(line1_p0, line0_p0, line0_p1);
    bool left4 = vec2_point_left_of_line(line1_p1, line0_p0, line0_p1);
    return (left1 != left2) && (left3 != left4);
}

bool vec2_point_in_polygon(vec2 point, int num_poly_points, vec2 *poly_points) {
    bool c = false;
    int i, j;
    for (i = 0, j = num_poly_points - 1; i < num_poly_points; j = i++) {
        if ( ((poly_points[i].y>point.y) != (poly_points[j].y>point.y)) &&
                (point.x < (poly_points[j].x-poly_points[i].x) * (point.y-poly_points[i].y) / 
                 (poly_points[j].y-poly_points[i].y) + poly_points[i].x) )
            c = !c;
    }
    return c;
}

vec2 vec2_bezier(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float t) {
    float t1 = (1.0f - t);
    vec2 v = vec2_scale(p0, t1 * t1 * t1);
    v = vec2_add(v, vec2_scale(p1, 3 * t * t1 * t1));
    v = vec2_add(v, vec2_scale(p2, 3 * t * t * t1));
    v = vec2_add(v, vec2_scale(p3, t * t * t));
    return v;
}

bool vec2_equal(vec2 v1, vec2 v2) {
    return (fabs(v1.x - v2.x) < 0.0001f) && (fabs(v1.y - v2.y) < 0.0001f);
}

void vec2_print(vec2 v) {
    printf("<%f, %f>\n", v.x, v.y);
}

vec3 vec3_create(float x, float y, float z) {
	vec3 v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

vec3 vec3_add(vec3 v1, vec3 v2) {
    return vec3_create(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z);
}

vec3 vec3_sub(vec3 v1, vec3 v2) {
    return vec3_create(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
}

vec3 vec3_add_scaled(vec3 v1, vec3 v2, float s) {
    return vec3_create(
        v1.x + v2.x * s,
        v1.y + v2.y * s,
        v1.z + v2.z * s);
}

vec3 vec3_subtract(vec3 v1, vec3 v2) {
    return vec3_create(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
}

vec3 vec3_normalize(vec3 v) {
    float l = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);

    if (l == 0.0) {
        return vec3_create(0.0, 0.0, 0.0);
    }

    return vec3_create(v.x / l, v.y / l, v.z / l);
}

float vec3_dot(vec3 v1, vec3 v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

vec3 vec3_cross(vec3 v1, vec3 v2) {
    float x = v1.y * v2.z - v1.z * v2.y;
    float y = v1.z * v2.x - v1.x * v2.z;
    float z = v1.x * v2.y - v1.y * v2.x;
    return vec3_create(x, y, z);
}

vec3 vec3_apply_quat(vec3 v, float w, quat q) {
    mat4 m = mat4_from_quat(q);
    return vec3_apply_mat4(v, w, m);
}

vec3 vec3_apply_mat4(vec3 v, float w, mat4 m) {
    float *a = m.m;
    float x = a[0] * v.x + a[1] * v.y + a[2] * v.z + w * a[3];
    float y = a[4] * v.x + a[5] * v.y + a[6] * v.z + w * a[7];
    float z = a[8] * v.x + a[9] * v.y + a[10] * v.z + w * a[11];
    return vec3_create(x, y, z);
}

vec3 vec3_scale(vec3 v, float s) {
    return vec3_create(s * v.x, s * v.y, s * v.z);
}

vec3 vec3_div(vec3 v, float s) {
    return vec3_create(v.x / s, v.y / s, v.z / s);
}

float vec3_distance(vec3 v1, vec3 v2) {
    return sqrtf((v1.x - v2.x) * (v1.x - v2.x) + (v1.y - v2.y) * (v1.y - v2.y) +
                 (v1.z - v2.z) * (v1.z - v2.z));
}

float vec3_distance_squared(vec3 v1, vec3 v2) {
    return (v1.x - v2.x) * (v1.x - v2.x) + (v1.y - v2.y) * (v1.y - v2.y) +
           (v1.z - v2.z) * (v1.z - v2.z);
}

vec3 vec3_set_length(vec3 v, float l) {
    return vec3_scale(vec3_normalize(v), l);
}

float vec3_length(vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

float vec3_length_squared(vec3 v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

bool vec3_equal(vec3 v1, vec3 v2) {
    return (fabs(v1.x - v2.x) <= 0.0001f) && (fabs(v1.y - v2.y) <= 0.0001f) && (fabs(v1.z - v2.z) <= 0.0001f);
}

vec3 vec3_rotate_x(vec3 v, float theta) {
    return vec3_rotate_about_axis(v, V3(1.0f, 0.0f, 0.0f), theta);
}

vec3 vec3_rotate_y(vec3 v, float theta) {
    return vec3_rotate_about_axis(v, V3(0.0f, 1.0f, 0.0f), theta);
}

vec3 vec3_rotate_z(vec3 v, float theta) {
    return vec3_rotate_about_axis(v, V3(0.0f, 0.0f, 1.0f), theta);
}

/*
 * http://inside.mines.edu/fs_home/gmurray/ArbitraryAxisRotation/
 */
vec3 vec3_rotate_about_axis(vec3 vec, vec3 axis, float theta) {
    float c = cosf(theta);
    float s = sinf(theta);

    float x = vec.x;
    float y = vec.y;
    float z = vec.z;

    float u = axis.x;
    float v = axis.y;
    float w = axis.z;

    vec3 rotated_vec;
    rotated_vec.x = u * (u * x + v * y + w * z) * (1.0f - c) + x * c + (-w * y + v * z) * s;
    rotated_vec.y = v * (u * x + v * y + w * z) * (1.0f - c) + y * c + (w * x - u * z) * s;
    rotated_vec.z = w * (u * x + v * y + w * z) * (1.0f - c) + z * c + (-v * x + u * y) * s;
    return rotated_vec;
}


vec3 vec3_reflect(vec3 u, vec3 v) {
    return vec3_subtract(vec3_perpindicular_component(u, v), vec3_parallel_component(u, v));
}

vec3 vec3_reflect_with_restitution(vec3 u, vec3 v, float e) {
    return vec3_subtract(vec3_perpindicular_component(u, v), vec3_scale(vec3_parallel_component(u, v), e));
}

vec3 vec3_parallel_component(vec3 u, vec3 v) {
    float l = vec3_dot(u, v) / vec3_length_squared(v);
    return vec3_scale(v, l);
}

vec3 vec3_perpindicular_component(vec3 u, vec3 v) {
    return vec3_subtract(u, vec3_parallel_component(u, v));
}

vec3 vec3_interpolate(vec3 v1, vec3 v2, float t) {
    vec3 v;
    v.x = v1.x + (v2.x - v1.x) * t;
    v.y = v1.y + (v2.y - v1.y) * t;
    v.z = v1.z + (v2.z - v1.z) * t;
    return v;
}

vec3 vec3_from_hex_color(int hex_color) {
    vec3 v;
    v.x = (float)(0xFF & (hex_color >> 16));
    v.y = (float)(0xFF & (hex_color >> 8));
    v.z = (float)(0xFF & (hex_color >> 0));
    return vec3_scale(v, 1.0f / 256.0f);
}

void vec3_print(vec3 v) {
    printf("<%f, %f, %f>\n", v.x, v.y, v.z);
}

vec3 vec3_multiply(vec3 v1, vec3 v2) {
    return V3(v1.x * v2.x, v1.y * v2.y, v1.z * v2.z);
}

vec3 vec3_orthogonal(vec3 u) {
    u = vec3_normalize(u);
    vec3 v = V3(1.0f, 0.0f, 0.0f);
    if (vec3_equal(u, v)) {
        u = V3(0.0f, 1.0f, 0.0f);
    }
    return vec3_cross(u, v);
}

float vec3_distance_squared_point_line_segment(vec3 p, vec3 a, vec3 b) {
    vec3 c = p;
    vec3 ab = vec3_sub(b, a);
    vec3 ac = vec3_sub(c, a);
    vec3 bc = vec3_sub(c, b);
    float e = vec3_dot(ac, ab);
    if (e <= 0.0f) return vec3_dot(ac, ac);
    float f = vec3_dot(ab, ab);
    if (e >= f) return vec3_dot(bc, bc);
    return vec3_dot(ac, ac) - e*e/f;
}

bool vec3_point_on_line_segment(vec3 p, vec3 a, vec3 b, float eps) {
    return vec3_distance_squared_point_line_segment(p, a, b) <= eps*eps;
}

bool vec3_line_segments_on_same_line(vec3 ap0, vec3 ap1, vec3 bp0, vec3 bp1, float eps) {
    // Check if triangle ap0, ap1, bp0 and ap0, ap1, bp1 both have area of 0
    vec3 v0 = vec3_cross(vec3_sub(ap0, ap1), vec3_sub(ap0, bp0));
    vec3 v1 = vec3_cross(vec3_sub(ap0, ap1), vec3_sub(ap0, bp1));
    return vec3_length_squared(v0) < eps*eps && vec3_length_squared(v1) < eps*eps;
}

vec4 vec4_create(float x, float y, float z, float w) {
    vec4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

vec4 vec4_add(vec4 a, vec4 b) {
    return V4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

vec4 vec4_sub(vec4 a, vec4 b) {
    return V4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

vec4 vec4_apply_mat(vec4 v, mat4 m) {
    float x = m.m[0] * v.x + m.m[1] * v.y + m.m[2] * v.z + m.m[3] * v.w;
    float y = m.m[4] * v.x + m.m[5] * v.y + m.m[6] * v.z + m.m[7] * v.w;
    float z = m.m[8] * v.x + m.m[9] * v.y + m.m[10] * v.z + m.m[11] * v.w;
    float w = m.m[12] * v.x + m.m[13] * v.y + m.m[14] * v.z + m.m[15] * v.w;
    return vec4_create(x, y, z, w);
}

vec4 vec4_scale(vec4 v, float s) {
    return V4(v.x * s, v.y * s, v.z * s, v.w * s);
}

void vec4_normalize_this(vec4 v) {
    float l = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
    if (l != 0) {
        v.x /= l;
        v.y /= l;
        v.z /= l;
        v.w /= l;
    }
}

void vec4_print(vec4 v) {
    printf("<%f, %f, %f, %f>\n", v.x, v.y, v.z, v.w);
}

mat4 mat4_create(float a, float b, float c, float d,
                 float e, float f, float g, float h,
                 float i, float j, float k, float l,
                 float m, float n, float o, float p) {
	mat4 mat;
	mat.m[0] = a;
	mat.m[1] = b;
	mat.m[2] = c;
	mat.m[3] = d;
	mat.m[4] = e;
	mat.m[5] = f;
	mat.m[6] = g;
	mat.m[7] = h;
	mat.m[8] = i;
	mat.m[9] = j;
	mat.m[10] = k;
	mat.m[11] = l;
	mat.m[12] = m;
	mat.m[13] = n;
	mat.m[14] = o;
	mat.m[15] = p;
	return mat;
}

mat4 mat4_zero() {
    return mat4_create(
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0);
}

mat4 mat4_identity() {
    return mat4_create(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0);
}

mat4 mat4_translation(vec3 v) {
    return mat4_create(
        1.0, 0.0, 0.0, v.x,
        0.0, 1.0, 0.0, v.y,
        0.0, 0.0, 1.0, v.z,
        0.0, 0.0, 0.0, 1.0);
}

mat4 mat4_scale(vec3 v) {
    return mat4_create(
        v.x, 0.0, 0.0, 0.0,
        0.0, v.y, 0.0, 0.0,
        0.0, 0.0, v.z, 0.0,
        0.0, 0.0, 0.0, 1.0);
}

/*
 * https://en.wikipedia.org/wiki/Rotation_matrix
 */
mat4 mat4_rotation_x(float theta) {
    float c = cosf(theta);
    float s = sinf(theta);
    return mat4_create(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, c, -s, 0.0f,
        0.0f, s, c, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

mat4 mat4_rotation_y(float theta) {
    float c = cosf(theta);
    float s = sinf(theta);
    return mat4_create(
        c, 0.0f, s, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -s, 0.0f, c, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

mat4 mat4_rotation_z(float theta) {
    float c = cosf(theta);
    float s = sinf(theta);
    return mat4_create(
        c, -s, 0.0f, 0.0f,
        s, c, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

mat4 mat4_shear(float theta, vec3 a, vec3 b) {
    float t = tanf(theta);
    return mat4_create(
        a.x * b.x * t + 1.0f, a.x * b.y * t, a.x * b.z * t, 0.0f,
        a.y * b.x * t, a.y * b.y * t + 1.0f, a.y * b.z * t, 0.0f,
        a.z * b.x * t, a.z * b.y * t, a.z * b.z * t + 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

void mat4_print(mat4 m) {
    float *a = m.m;
    printf("[%f, %f, %f, %f]\n", a[0], a[1], a[2], a[3]);
    printf("[%f, %f, %f, %f]\n", a[4], a[5], a[6], a[7]);
    printf("[%f, %f, %f, %f]\n", a[8], a[9], a[10], a[11]);
    printf("[%f, %f, %f, %f]\n", a[12], a[13], a[14], a[15]);
}

mat4 mat4_multiply_n(int count, ...) {
    va_list args;
    va_start(args, count);
    mat4 result = mat4_identity();
    for (int i = 0; i < count; i++) {
        result = mat4_multiply(result, va_arg(args, mat4));
    }
    va_end(args);
    return result;
}

mat4 mat4_multiply(mat4 m1, mat4 m2) {
    float *a = m1.m;
    float *b = m2.m;

    float c0 = a[0] * b[0] + a[1] * b[4] + a[2] * b[8] + a[3] * b[12];
    float c1 = a[0] * b[1] + a[1] * b[5] + a[2] * b[9] + a[3] * b[13];
    float c2 = a[0] * b[2] + a[1] * b[6] + a[2] * b[10] + a[3] * b[14];
    float c3 = a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + a[3] * b[15];

    float c4 = a[4] * b[0] + a[5] * b[4] + a[6] * b[8] + a[7] * b[12];
    float c5 = a[4] * b[1] + a[5] * b[5] + a[6] * b[9] + a[7] * b[13];
    float c6 = a[4] * b[2] + a[5] * b[6] + a[6] * b[10] + a[7] * b[14];
    float c7 = a[4] * b[3] + a[5] * b[7] + a[6] * b[11] + a[7] * b[15];

    float c8 = a[8] * b[0] + a[9] * b[4] + a[10] * b[8] + a[11] * b[12];
    float c9 = a[8] * b[1] + a[9] * b[5] + a[10] * b[9] + a[11] * b[13];
    float c10 = a[8] * b[2] + a[9] * b[6] + a[10] * b[10] + a[11] * b[14];
    float c11 = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11] * b[15];

    float c12 = a[12] * b[0] + a[13] * b[4] + a[14] * b[8] + a[15] * b[12];
    float c13 = a[12] * b[1] + a[13] * b[5] + a[14] * b[9] + a[15] * b[13];
    float c14 = a[12] * b[2] + a[13] * b[6] + a[14] * b[10] + a[15] * b[14];
    float c15 = a[12] * b[3] + a[13] * b[7] + a[14] * b[11] + a[15] * b[15];

    return mat4_create(
        c0, c1, c2, c3,
        c4, c5, c6, c7,
        c8, c9, c10, c11,
        c12, c13, c14, c15);
}

mat4 mat4_transpose(mat4 m) {
    float *a = m.m;

    return mat4_create(
        a[0], a[4], a[8], a[12],
        a[1], a[5], a[9], a[13],
        a[2], a[6], a[10], a[14],
        a[3], a[7], a[11], a[15]);
}

mat4 mat4_normal_transform(mat4 m) {
    return mat4_transpose(mat4_inverse(m));
}

/*
 * http://www.cg.info.hiroshima-cu.ac.jp/~miyazaki/knowledge/teche23.html
 */
mat4 mat4_inverse(mat4 m) {
    float *a = m.m;

    float det =
        a[0] * a[5] * a[10] * a[15] +
        a[0] * a[6] * a[11] * a[13] +
        a[0] * a[7] * a[9] * a[14] +

        a[1] * a[4] * a[11] * a[14] +
        a[1] * a[6] * a[8] * a[15] +
        a[1] * a[7] * a[10] * a[12] +

        a[2] * a[4] * a[9] * a[15] +
        a[2] * a[5] * a[11] * a[12] +
        a[2] * a[7] * a[8] * a[13] +

        a[3] * a[4] * a[10] * a[13] +
        a[3] * a[5] * a[8] * a[14] +
        a[3] * a[6] * a[9] * a[12] -

        a[0] * a[5] * a[11] * a[14] -
        a[0] * a[6] * a[9] * a[15] -
        a[0] * a[7] * a[10] * a[13] -

        a[1] * a[4] * a[10] * a[15] -
        a[1] * a[6] * a[11] * a[12] -
        a[1] * a[7] * a[8] * a[14] -

        a[2] * a[4] * a[11] * a[13] -
        a[2] * a[5] * a[8] * a[15] -
        a[2] * a[7] * a[9] * a[12] -

        a[3] * a[4] * a[9] * a[14] -
        a[3] * a[5] * a[10] * a[12] -
        a[3] * a[6] * a[8] * a[13];

    if (det == 0.0) {
        return mat4_identity();
    }

    float a0 = a[5] * a[10] * a[15] + a[6] * a[11] * a[13] + a[7] * a[9] * a[14] - a[5] * a[11] * a[14] - a[6] * a[9] * a[15] - a[7] * a[10] * a[13];

    float a1 = a[1] * a[11] * a[14] + a[2] * a[9] * a[15] + a[3] * a[10] * a[13] - a[1] * a[10] * a[15] - a[2] * a[11] * a[13] - a[3] * a[9] * a[14];

    float a2 = a[1] * a[6] * a[15] + a[2] * a[7] * a[13] + a[3] * a[5] * a[14] - a[1] * a[7] * a[14] - a[2] * a[5] * a[15] - a[3] * a[6] * a[13];

    float a3 = a[1] * a[7] * a[10] + a[2] * a[5] * a[11] + a[3] * a[6] * a[9] - a[1] * a[6] * a[11] - a[2] * a[7] * a[9] - a[3] * a[5] * a[10];

    float a4 = a[4] * a[11] * a[14] + a[6] * a[8] * a[15] + a[7] * a[10] * a[12] - a[4] * a[10] * a[15] - a[6] * a[11] * a[12] - a[7] * a[8] * a[14];

    float a5 = a[0] * a[10] * a[15] + a[2] * a[11] * a[12] + a[3] * a[8] * a[14] - a[0] * a[11] * a[14] - a[2] * a[8] * a[15] - a[3] * a[10] * a[12];

    float a6 = a[0] * a[7] * a[14] + a[2] * a[4] * a[15] + a[3] * a[6] * a[12] - a[0] * a[6] * a[15] - a[2] * a[7] * a[12] - a[3] * a[4] * a[14];

    float a7 = a[0] * a[6] * a[11] + a[2] * a[7] * a[8] + a[3] * a[4] * a[10] - a[0] * a[7] * a[10] - a[2] * a[4] * a[11] - a[3] * a[6] * a[8];

    float a8 = a[4] * a[9] * a[15] + a[5] * a[11] * a[12] + a[7] * a[8] * a[13] - a[4] * a[11] * a[13] - a[5] * a[8] * a[15] - a[7] * a[9] * a[12];

    float a9 = a[0] * a[11] * a[13] + a[1] * a[8] * a[15] + a[3] * a[9] * a[12] - a[0] * a[9] * a[15] - a[1] * a[11] * a[12] - a[3] * a[8] * a[13];

    float a10 = a[0] * a[5] * a[15] + a[1] * a[7] * a[12] + a[3] * a[4] * a[13] - a[0] * a[7] * a[13] - a[1] * a[4] * a[15] - a[3] * a[5] * a[12];

    float a11 = a[0] * a[7] * a[9] + a[1] * a[4] * a[11] + a[3] * a[5] * a[8] - a[0] * a[5] * a[11] - a[1] * a[7] * a[8] - a[3] * a[4] * a[9];

    float a12 = a[4] * a[10] * a[13] + a[5] * a[8] * a[14] + a[6] * a[9] * a[12] - a[4] * a[9] * a[14] - a[5] * a[10] * a[12] - a[6] * a[8] * a[13];

    float a13 = a[0] * a[9] * a[14] + a[1] * a[10] * a[12] + a[2] * a[8] * a[13] - a[0] * a[10] * a[13] - a[1] * a[8] * a[14] - a[2] * a[9] * a[12];

    float a14 = a[0] * a[6] * a[13] + a[1] * a[4] * a[14] + a[2] * a[5] * a[12] - a[0] * a[5] * a[14] - a[1] * a[6] * a[12] - a[2] * a[4] * a[13];

    float a15 = a[0] * a[5] * a[10] + a[1] * a[6] * a[8] + a[2] * a[4] * a[9] - a[0] * a[6] * a[9] - a[1] * a[4] * a[10] - a[2] * a[5] * a[8];

    return mat4_create(
        a0 / det, a1 / det, a2 / det, a3 / det,
        a4 / det, a5 / det, a6 / det, a7 / det,
        a8 / det, a9 / det, a10 / det, a11 / det,
        a12 / det, a13 / det, a14 / det, a15 / det);
}

/*
 * http://www.cs.virginia.edu/~gfx/Courses/1999/intro.fall99.html/lookat.html
 */
mat4 mat4_look_at(vec3 position, vec3 target, vec3 up) {
    vec3 up_p = vec3_normalize(up);
    vec3 f = vec3_normalize(vec3_subtract(target, position));
    vec3 s = vec3_normalize(vec3_cross(f, up_p));
    vec3 u = vec3_normalize(vec3_cross(s, f));

    mat4 M = mat4_create(
        s.x, s.y, s.z, 0.0,
        u.x, u.y, u.z, 0.0,
        -f.x, -f.y, -f.z, 0.0,
        0.0, 0.0, 0.0, 1.0);
    mat4 T = mat4_translation(vec3_scale(position, -1.0));

    return mat4_multiply(M, T);
}

mat4 mat4_from_axes(vec3 x, vec3 y, vec3 z) {
    return mat4_create(
        x.x, x.y, x.z, 0.0f,
        y.x, y.y, y.z, 0.0f,
        z.x, z.y, z.z, 0.0f,
        0.0, 0.0, 0.0, 1.0f);
}

void mat4_get_axes(mat4 m, vec3 *x, vec3 *y, vec3 *z) {
    x->x = m.m[0];
    x->y = m.m[1];
    x->z = m.m[2];

    y->x = m.m[4];
    y->y = m.m[5];
    y->z = m.m[6];

    z->x = m.m[8];
    z->y = m.m[9];
    z->z = m.m[10];
}

/*
 * http://www.cs.virginia.edu/~gfx/Courses/2000/intro.spring00.html/lectures/lecture09/sld017.htm
 */
mat4 mat4_perspective_projection(float fov, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov * DEGREES_PER_RADIAN / 2.0f);
    float denominator = near - far;

    float a = f / aspect;
    float b = f;
    float c = (far + near) / denominator;
    float d = (2.0f * far * near) / denominator;

    return mat4_create(
        a, 0.0f, 0.0f, 0.0f,
        0.0f, b, 0.0f, 0.0f,
        0.0f, 0.0f, c, d,
        0.0f, 0.0f, -1.0f, 0.0f);
}

/*
 * https://en.wikipedia.org/wiki/Orthographic_projection
 */
mat4 mat4_orthographic_projection(float left, float right, float bottom, float top,
        float near, float far) {
    float a = 2.0f / (right - left);
    float b = 2.0f / (top - bottom);
    float c = -2.0f / (far - near);
    float d = -(right + left) / (right - left);
    float e = -(top + bottom) / (top - bottom);
    float f = -(far + near) / (far - near);

    return mat4_create(
        a, 0.0f, 0.0f, d,
        0.0f, b, 0.0f, e,
        0.0f, 0.0f, c, f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

mat4 mat4_from_quat(quat q) {
    float x = q.x;
    float y = q.y;
    float z = q.z;
    float w = q.w;

    return mat4_create(
        1.0f - 2.0f * y * y - 2.0f * z * z, 2.0f * x * y - 2.0f * w * z, 2.0f * x * z + 2.0f * w * y,
        0.0f,
        2.0f * x * y + 2.0f * w * z, 1.0f - 2.0f * x * x - 2.0f * z * z, 2.0f * y * z - 2.0f * w * x,
        0.0f,
        2.0f * x * z - 2.0f * w * y, 2.0f * y * z + 2.0f * w * x, 1.0f - 2.0f * x * x - 2.0f * y * y,
        0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

mat4 mat4_box_inertia_tensor(vec3 half_lengths, float mass) {
    return mat4_create(
        (1.0f / 12.0f) * mass * (half_lengths.x + half_lengths.y), 0.0f, 0.0f, 0.0f,
        0.0f, (1.0f / 12.0f) * mass * (half_lengths.y + half_lengths.z), 0.0f, 0.0f,
        0.0f, 0.0f, (1.0f / 12.0f) * mass * (half_lengths.z + half_lengths.x), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

mat4 mat4_sphere_inertia_tensor(float radius, float mass) {
    return mat4_create(
        (2.0f / 5.0f) * mass * radius * radius, 0.0f, 0.0f, 0.0f,
        0.0f, (2.0f / 5.0f) * mass * radius * radius, 0.0f, 0.0f,
        0.0f, 0.0f, (2.0f / 5.0f) * mass * radius * radius, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

mat4 mat4_triangle_transform(vec2 src_p1, vec2 src_p2, vec2 src_p3,
                             vec2 dest_p1, vec2 dest_p2, vec2 dest_p3) {
    float x11 = src_p1.x;
    float x12 = src_p1.y;
    float x21 = src_p2.x;
    float x22 = src_p2.y;
    float x31 = src_p3.x;
    float x32 = src_p3.y;
    float y11 = dest_p1.x;
    float y12 = dest_p1.y;
    float y21 = dest_p2.x;
    float y22 = dest_p2.y;
    float y31 = dest_p3.x;
    float y32 = dest_p3.y;

    float a1 = ((y11 - y21) * (x12 - x32) - (y11 - y31) * (x12 - x22)) /
               ((x11 - x21) * (x12 - x32) - (x11 - x31) * (x12 - x22));
    float a2 = ((y11 - y21) * (x11 - x31) - (y11 - y31) * (x11 - x21)) /
               ((x12 - x22) * (x11 - x31) - (x12 - x32) * (x11 - x21));
    float a3 = y11 - a1 * x11 - a2 * x12;
    float a4 = ((y12 - y22) * (x12 - x32) - (y12 - y32) * (x12 - x22)) /
               ((x11 - x21) * (x12 - x32) - (x11 - x31) * (x12 - x22));
    float a5 = ((y12 - y22) * (x11 - x31) - (y12 - y32) * (x11 - x21)) /
               ((x12 - x22) * (x11 - x31) - (x12 - x32) * (x11 - x21));
    float a6 = y12 - a4 * x11 - a5 * x12;

    return mat4_create(a1, a2, a3, 0.0f,
                       a4, a5, a6, 0.0f,
                       0.0f, 0.0f, 1.0f, 0.0f,
                       0.0f, 0.0f, 0.0f, 1.0f);
}

mat4 mat4_box_to_line_transform(vec3 p0, vec3 p1, float sz) {
    float dist = vec3_distance(p0, p1); 
    vec3 v0 = V3(1.0f, 0.0f, 0.0f);
    vec3 v1 = vec3_normalize(vec3_subtract(p1, p0));
    quat q = quat_between_vectors(v0, v1);

    return mat4_multiply_n(4,
            mat4_translation(p0),
            mat4_from_quat(q),
            mat4_scale(V3(0.5f * dist, 0.5f * sz, 0.5f * sz)),
            mat4_translation(V3(1.0f, 0.0f, 0.0f)));
}

mat4 mat4_interpolate(mat4 m0, mat4 m1, float t) {
    mat4 m;
    for (int i = 0; i < 16; i++) {
        m.m[i] = m0.m[i] + (m1.m[i] - m0.m[i]) * t;
    }
    return m;
}

quat quat_create(float x, float y, float z, float w) {
	quat q;
	q.x = x;
	q.y = y;
	q.z = z;
	q.w = w;
	return q;
}

quat quat_create_from_axis_angle(vec3 axis, float angle) {
    axis = vec3_normalize(axis);
    float temp = sinf(angle / 2.0f);
    float x = temp * axis.x;
    float y = temp * axis.y;
    float z = temp * axis.z;
    float w = cosf(angle / 2.0f);
    return quat_create(x, y, z, w);
}

quat quat_multiply(quat u, quat v) {
    float x = v.w * u.x + v.x * u.w - v.y * u.z + v.z * u.y;
    float y = v.w * u.y + v.x * u.z + v.y * u.w - v.z * u.x;
    float z = v.w * u.z - v.x * u.y + v.y * u.x + v.z * u.w;
    float w = v.w * u.w - v.x * u.x - v.y * u.y - v.z * u.z;
    return quat_normalize(quat_create(x, y, z, w));
}

quat quat_normalize(quat q) {
    float mag = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);

    if (mag == 0.0f) {
        return q;
    }

    return quat_create(q.x / mag, q.y / mag, q.z / mag, q.w / mag);
}

quat quat_interpolate(quat u, quat v, float a) {
    quat r;
    u = quat_normalize(u);
    v = quat_normalize(v);

    r.x = u.x + (v.x - u.x) * a;
    r.y = u.y + (v.y - u.y) * a;
    r.z = u.z + (v.z - u.z) * a;
    r.w = u.w + (v.w - u.w) * a;

    return quat_normalize(r);
}

quat quat_subtract(quat q1, quat q2) {
    return QUAT(q1.x - q2.x, q1.y - q2.y, q1.z - q2.z, q1.w - q2.w);
}

quat quat_add(quat q1, quat q2) {
    return QUAT(q1.x + q2.x, q1.y + q2.y, q1.z + q2.z, q1.w + q2.w);
}

float quat_dot(quat v0, quat v1) {
    return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z + v0.w * v1.w;
}

quat quat_scale(quat v, float s) {
    return QUAT(v.x * s, v.y * s, v.z * s, v.w * s);
}

quat quat_slerp(quat v0, quat v1, float a) {
    v0 = quat_normalize(v0);
    v1 = quat_normalize(v1);

    float dot = quat_dot(v0, v1);
    if (fabs(dot) > 0.95f) {
        return quat_add(v0, quat_scale(quat_subtract(v1, v0), a));
    }

    if (dot < 0.0f) {
        v1 = quat_scale(v1, -1.0f);
        dot = -dot;
    }

    dot = fminf(fmaxf(-1.0f, dot), 1.0f);
    float theta_0 = acosf(dot);
    float theta = theta_0 * a;

    quat v2 = quat_subtract(v1, quat_scale(v0, a));
    v2 = quat_normalize(v2);

    return quat_add(quat_scale(v0, cosf(theta)), quat_scale(v2, sinf(theta)));
}

quat quat_between_vectors(vec3 u, vec3 v) {
    float k_cos_theta = vec3_dot(u, v);
    float k = sqrtf(vec3_length_squared(u) * vec3_length_squared(v));

    if (k_cos_theta / k == -1.0f) {
        vec3 orth = vec3_normalize(vec3_orthogonal(u));
        return QUAT(orth.x, orth.y, orth.z, 0.0f);
    }

    vec3 cross = vec3_cross(u, v);
    return quat_normalize(QUAT(cross.x, cross.y, cross.z, k_cos_theta + k));
}

void quat_get_axes(quat q, vec3 *x, vec3 *y, vec3 *z) {
    mat4 m = mat4_from_quat(q);

    x->x = m.m[0];
    y->x = m.m[1];
    z->x = m.m[2];

    x->y = m.m[4];
    y->y = m.m[5];
    z->y = m.m[6];

    x->z = m.m[8];
    y->z = m.m[9];
    z->z = m.m[10];
}

void quat_get_axis_angle(quat q, vec3 *axis, float *angle) {
    *angle = 2.0f * acosf(q.w);
    axis->x = q.x / sqrtf(1.0f - q.w*q.w);
    axis->y = q.y / sqrtf(1.0f - q.w*q.w);
    axis->z = q.z / sqrtf(1.0f - q.w*q.w);
}

void quat_print(quat q) {
    printf("<%f, %f, %f, %f>\n", q.x, q.y, q.z, q.w);
}

struct rect_2D rect_2D_create(vec2 tl, vec2 sz) {
    struct rect_2D rect;
    rect.top_left = tl;
    rect.size = sz;
    return rect;
}

bool rect_2D_contains_point(struct rect_2D rect, vec2 pt) {
    vec2 p0 = rect.top_left;
    vec2 p1 = V2(p0.x + rect.size.x, p0.y - rect.size.y);
    return pt.x >= p0.x && pt.x <= p1.x &&
           pt.y <= p0.y && pt.y >= p1.y;
}

static bool intersect_segment_triangle(vec3 p, vec3 q, vec3 a, vec3 b, vec3 c, float *t) {
    vec3 ab = vec3_subtract(b, a);
    vec3 ac = vec3_subtract(c, a);
    vec3 qp = vec3_subtract(p, q);

    vec3 n = vec3_cross(ab, ac);

    float d = vec3_dot(qp, n);
    if (d <= 0.0f) return false;

    vec3 ap = vec3_subtract(p, a);
    *t = vec3_dot(ap, n);
    if (*t < 0.0f) return false;
    //if (*t > d) return false;

    vec3 e = vec3_cross(qp, ap);
    float v = vec3_dot(ac, e);
    if (v < 0.0f || v > d) return false;
    float w = -vec3_dot(ab, e);
    if (w < 0.0f || v + w > d) return false;

    float ood = 1.0f / d;
    *t *= ood;
    return true;
}

bool point_inside_box(vec3 p, vec3 box_center, vec3 box_half_lengths) {
    vec3 c = box_center;
    vec3 e = box_half_lengths;
    return (p.x >= c.x - e.x) && (p.x <= c.x + e.x) &&
        (p.y >= c.y - e.y) && (p.y <= c.y + e.y) &&
        (p.z >= c.z - e.z) && (p.z <= c.z + e.z);
}

void ray_intersect_triangles_all(vec3 ro, vec3 rd, vec3 *points, int num_points, mat4 transform, float *t) { 
    for (int i = 0; i < num_points; i += 3) {
        vec3 tp0 = points[i + 0];
        vec3 tp1 = points[i + 1];
        vec3 tp2 = points[i + 2];
        tp0 = vec3_apply_mat4(tp0, 1.0f, transform);
        tp1 = vec3_apply_mat4(tp1, 1.0f, transform);
        tp2 = vec3_apply_mat4(tp2, 1.0f, transform);

        float t0;
        if (intersect_segment_triangle(ro, vec3_add(ro, rd), tp0, tp1, tp2, &t0)) {
            t[i/3] = t0;    
        }
        else {
            t[i/3] = FLT_MAX;
        }
    }
}

bool ray_intersect_triangles(vec3 ro, vec3 rd, vec3 *points, int num_points, mat4 transform, float *t, int *idx) {
    float min_t = FLT_MAX;

    for (int i = 0; i < num_points; i += 3) {
        vec3 tp0 = points[i + 0];
        vec3 tp1 = points[i + 1];
        vec3 tp2 = points[i + 2];
        tp0 = vec3_apply_mat4(tp0, 1.0f, transform);
        tp1 = vec3_apply_mat4(tp1, 1.0f, transform);
        tp2 = vec3_apply_mat4(tp2, 1.0f, transform);

        float t0;
        if (intersect_segment_triangle(ro, vec3_add(ro, rd), tp0, tp1, tp2, &t0)) {
            if (t0 < min_t) {
                min_t = t0;
                *idx = (i / 3);
            }
        }
    }

    *t = min_t;
    return min_t < FLT_MAX;
}

bool ray_intersect_spheres(vec3 ro, vec3 rd, vec3 *center, float *radius, int num_spheres, float *t, int *idx) {
    float min_t = FLT_MAX; 

    for (int i = 0; i < num_spheres; i++) {
        vec3 oc = vec3_subtract(ro, center[i]);
        float a = vec3_dot(rd, rd);
        float b = 2.0f * vec3_dot(oc, rd);
        float c = vec3_dot(oc, oc) - radius[i] * radius[i];
        float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f) {
        } else {
            float t0 = (-b - sqrtf(disc)) / (2.0f * a);
            if (t0 < min_t) {
                min_t = t0;
                *idx = i;
            }
        }
    }

    *t = min_t;
    return min_t < FLT_MAX;
}

bool ray_intersect_segments(vec3 ro, vec3 rd, vec3 *p0, vec3 *p1, float radius, int num_segments, float *t, int *idx) {
    float min_t = FLT_MAX;

    for (int i = 0; i < num_segments; i++) {
        float s0, t0;
        vec3 c0, c1;
        float dist = closest_point_ray_segment(ro, vec3_add(ro, rd), p0[i], p1[i], &s0, &t0, &c0, &c1);
        if (dist < radius * radius) {
            if (s0 < min_t) {
                min_t = s0;
                *idx = i;
            }
        }
    }

    *t = min_t;
    return min_t < FLT_MAX;
}

bool ray_intersect_planes(vec3 ro, vec3 rd, vec3 *p, vec3 *n, int num_planes, float *t, int *idx) {
    float min_t = FLT_MAX;

    for (int i = 0; i < num_planes; i++) {
        float denom = vec3_dot(rd, n[i]);
        if (denom != 0.0f) {
            float t = vec3_dot(n[i], vec3_sub(p[i], ro)) / denom;
            if (t < min_t) {
                min_t = t;
                *idx = i;
            }
        } else if (vec3_dot(vec3_subtract(ro, p[i]), n[i]) == 0.0f) {
            if (0.0f < min_t) {
                min_t = 0.0f;
                *idx = i;
            }
        }
    }

    *t = min_t;
    return min_t < FLT_MAX;
}

bool ray_intersect_box(vec3 ro, vec3 rd, vec3 box_center, vec3 box_half_lengths, float *t) {
    vec3 min = vec3_sub(box_center, box_half_lengths);
    vec3 max = vec3_add(box_center, box_half_lengths);

    float tmin = (min.x - ro.x) / rd.x;
    float tmax = (max.x - ro.x) / rd.x;

    if (tmin > tmax) {
        float temp = tmin;
        tmin = tmax;
        tmax = temp;
    }

    float tymin = (min.y - ro.y) / rd.y;
    float tymax = (max.y - ro.y) / rd.y;

    if (tymin > tymax) {
        float temp = tymin;
        tymin = tymax;
        tymax = temp;
    }

    if ((tmin > tymax) || (tymin > tmax))
        return false;

    if (tymin > tmin)
        tmin = tymin;

    if (tymax < tmax)
        tmax = tymax;

    float tzmin = (min.z - ro.z) / rd.z;
    float tzmax = (max.z - ro.z) / rd.z;

    if (tzmin > tzmax) {
        float temp = tzmin;
        tzmin = tzmax;
        tzmax = temp;
    }

    if ((tmin > tzmax) || (tzmin > tmax))
        return false;

    if (tzmin > tmin)
        tmin = tzmin;

    if (tzmax < tmax)
        tmax = tzmax;

    *t = tmin;

    return true;
}

void triangles_inside_box(vec3 *triangle_points, int num_triangles, vec3 box_center, vec3 box_half_lengths,
        bool *is_inside) {
    vec3 bc = box_center;
    vec3 be = box_half_lengths;
    vec3 bn[3] = {
        V3(1.0f, 0.0f, 0.0f),
        V3(0.0f, 1.0f, 0.0f),
        V3(0.0f, 0.0f, 1.0f),
    };
    vec3 bp[8] = {
        V3(bc.x - be.x, bc.y - be.y, bc.z - be.z),
        V3(bc.x - be.x, bc.y - be.y, bc.z + be.z),
        V3(bc.x - be.x, bc.y + be.y, bc.z - be.z),
        V3(bc.x - be.x, bc.y + be.y, bc.z + be.z),
        V3(bc.x + be.x, bc.y - be.y, bc.z - be.z),
        V3(bc.x + be.x, bc.y - be.y, bc.z + be.z),
        V3(bc.x + be.x, bc.y + be.y, bc.z - be.z),
        V3(bc.x + be.x, bc.y + be.y, bc.z + be.z),
    };

    for (int i = 0; i < num_triangles; i++) {
        vec3 tp[3] = {
            triangle_points[3*i + 0],
            triangle_points[3*i + 1],
            triangle_points[3*i + 2],
        };
        vec3 axes[13] = {
            // box face normals
            bn[0], bn[1], bn[2],

            // triangle face normal
            vec3_cross(vec3_sub(tp[1], tp[0]), vec3_sub(tp[2], tp[1])),

            vec3_cross(vec3_sub(tp[1], tp[0]), bn[0]),
            vec3_cross(vec3_sub(tp[1], tp[0]), bn[1]),
            vec3_cross(vec3_sub(tp[1], tp[0]), bn[2]),

            vec3_cross(vec3_sub(tp[2], tp[1]), bn[0]),
            vec3_cross(vec3_sub(tp[2], tp[1]), bn[1]),
            vec3_cross(vec3_sub(tp[2], tp[1]), bn[2]),

            vec3_cross(vec3_sub(tp[2], tp[0]), bn[0]),
            vec3_cross(vec3_sub(tp[2], tp[0]), bn[1]),
            vec3_cross(vec3_sub(tp[2], tp[0]), bn[2]),
        };

        is_inside[i] = true;
        for (int j = 0; j < 13; j++) {
            float min1 = FLT_MAX, min2 = FLT_MAX;
            float max1 = -FLT_MAX, max2 = -FLT_MAX;
            for (int k = 0; k < 3; k++) {
                float dot = vec3_dot(axes[j], tp[k]);
                min1 = dot < min1 ? dot : min1;
                max1 = dot > max1 ? dot : max1;
            }
            for (int k = 0; k < 8; k++) {
                float dot = vec3_dot(axes[j], bp[k]);
                min2 = dot < min2 ? dot : min2;
                max2 = dot > max2 ? dot : max2;
            }
            if (min1 > max2 || max1 < min2) {
                is_inside[i] = false;
                break;
            }
        }
    }
}

void triangles_inside_frustum(vec3 *triangle_points, int num_triangles, vec3 *frustum_corners, bool *is_inside) {
    vec3 *fc = frustum_corners;
    for (int i = 0; i < num_triangles; i++) {
        vec3 tp[3] = {
            triangle_points[3 * i + 0],
            triangle_points[3 * i + 1],
            triangle_points[3 * i + 2],
        };

        vec3 axes[24] = {
            //faces
            vec3_cross(vec3_sub(tp[2], tp[0]), vec3_sub(tp[1], tp[0])),
            vec3_cross(vec3_sub(fc[2], fc[0]), vec3_sub(fc[1], fc[0])),
            vec3_cross(vec3_sub(fc[5], fc[1]), vec3_sub(fc[6], fc[1])),
            vec3_cross(vec3_sub(fc[6], fc[2]), vec3_sub(fc[7], fc[2])),
            vec3_cross(vec3_sub(fc[7], fc[3]), vec3_sub(fc[4], fc[3])),
            vec3_cross(vec3_sub(fc[4], fc[0]), vec3_sub(fc[5], fc[0])),

            //edges
            vec3_cross(vec3_sub(tp[1], tp[0]), vec3_sub(fc[1], fc[0])),
            vec3_cross(vec3_sub(tp[1], tp[0]), vec3_sub(fc[3], fc[0])),
            vec3_cross(vec3_sub(tp[1], tp[0]), vec3_sub(fc[4], fc[0])),
            vec3_cross(vec3_sub(tp[1], tp[0]), vec3_sub(fc[5], fc[1])),
            vec3_cross(vec3_sub(tp[1], tp[0]), vec3_sub(fc[6], fc[2])),
            vec3_cross(vec3_sub(tp[1], tp[0]), vec3_sub(fc[7], fc[3])),

            vec3_cross(vec3_sub(tp[2], tp[0]), vec3_sub(fc[1], fc[0])),
            vec3_cross(vec3_sub(tp[2], tp[0]), vec3_sub(fc[3], fc[0])),
            vec3_cross(vec3_sub(tp[2], tp[0]), vec3_sub(fc[4], fc[0])),
            vec3_cross(vec3_sub(tp[2], tp[0]), vec3_sub(fc[5], fc[1])),
            vec3_cross(vec3_sub(tp[2], tp[0]), vec3_sub(fc[6], fc[2])),
            vec3_cross(vec3_sub(tp[2], tp[0]), vec3_sub(fc[7], fc[3])),

            vec3_cross(vec3_sub(tp[2], tp[1]), vec3_sub(fc[1], fc[0])),
            vec3_cross(vec3_sub(tp[2], tp[1]), vec3_sub(fc[3], fc[0])),
            vec3_cross(vec3_sub(tp[2], tp[1]), vec3_sub(fc[4], fc[0])),
            vec3_cross(vec3_sub(tp[2], tp[1]), vec3_sub(fc[5], fc[1])),
            vec3_cross(vec3_sub(tp[2], tp[1]), vec3_sub(fc[6], fc[2])),
            vec3_cross(vec3_sub(tp[2], tp[1]), vec3_sub(fc[7], fc[3])),
        };

        is_inside[i] = true;
        for (int j = 0; j < 24; j++) {
            float min1 = FLT_MAX, min2 = FLT_MAX;
            float max1 = -FLT_MAX, max2 = -FLT_MAX;
            for (int k = 0; k < 3; k++) {
                float dot = vec3_dot(axes[j], tp[k]);
                min1 = dot < min1 ? dot : min1;
                max1 = dot > max1 ? dot : max1;
            }
            for (int k = 0; k < 8; k++) {
                float dot = vec3_dot(axes[j], fc[k]);
                min2 = dot < min2 ? dot : min2;
                max2 = dot > max2 ? dot : max2;
            }
            if (min1 > max2 || max1 < min2) {
                is_inside[i] = false;
                break;
            }
        }
    }
}

vec3 closest_point_point_plane(vec3 point, vec3 plane_point, vec3 plane_normal) {
    float t = vec3_dot(plane_normal, vec3_subtract(point, plane_point)) / vec3_dot(plane_normal, plane_normal);
    return vec3_subtract(point, vec3_scale(plane_normal, t));
}

vec3 closest_point_point_circle(vec3 point, vec3 circle_center, vec3 circle_normal, float circle_radius) {
    vec3 closest_point_in_plane = closest_point_point_plane(point, circle_center, circle_normal);
    float r = circle_radius;
    return vec3_add(circle_center, vec3_scale(vec3_normalize(vec3_subtract(closest_point_in_plane, circle_center)), r));
}

vec3 closest_point_point_triangle(vec3 p, vec3 a, vec3 b, vec3 c, enum triangle_contact_type *type) {
    vec3 ab = vec3_subtract(b, a);
    vec3 ac = vec3_subtract(c, a);
    vec3 ap = vec3_subtract(p, a);
    float d1 = vec3_dot(ab, ap);
    float d2 = vec3_dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        *type = TRIANGLE_CONTACT_A;
        return a;
    }

    vec3 bp = vec3_subtract(p, b);
    float d3 = vec3_dot(ab, bp);
    float d4 = vec3_dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) {
        *type = TRIANGLE_CONTACT_B;
        return b; 
    }

    float vc = d1*d4 - d3*d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        *type = TRIANGLE_CONTACT_AB;
        float v = d1 / (d1 - d3);
        return vec3_add(a, vec3_scale(ab, v));
    }

    vec3 cp = vec3_subtract(p, c);
    float d5 = vec3_dot(ab, cp);
    float d6 = vec3_dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) {
        *type = TRIANGLE_CONTACT_C;
        return c;
    }

    float vb = d5*d2 - d1*d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        *type = TRIANGLE_CONTACT_AC;
        float w = d2 / (d2 - d6);
        return vec3_add(a, vec3_scale(ac, w));
    }

    float va = d3*d6 - d5*d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        *type = TRIANGLE_CONTACT_BC;
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return vec3_add(b, vec3_scale(vec3_subtract(c, b), w));
    }

    *type = TRIANGLE_CONTACT_FACE;
    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return vec3_add(vec3_add(a, vec3_scale(ab, v)), vec3_scale(ac, w));
}

vec3 closest_point_point_obb(vec3 p, vec3 bc, vec3 bx, vec3 by, vec3 bz, float bex, float bey, float bez) {
    vec3 bu[3] = {bx, by, bz};
    float be[3] = {bex, bey, bez};
    vec3 d = vec3_sub(p, bc);    
    vec3 q = bc;
    for (int i = 0; i < 3; i++) {
        float dist = vec3_dot(d, bu[i]);
        if (dist > be[i]) dist = be[i];
        if (dist < -be[i]) dist = -be[i];
        q = vec3_add(q, vec3_scale(bu[i], dist));
    }
    return q;
}

float clamp(float n, float min, float max) {
    if (n < min) return min;
    if (n > max) return max;
    return n;
}

float closest_point_ray_segment(vec3 p1, vec3 q1, vec3 p2, vec3 q2, float *s, float *t, vec3 *c1, vec3 *c2) {
    vec3 d1 = vec3_sub(q1, p1);
    vec3 d2 = vec3_sub(q2, p2);
    vec3 r = vec3_sub(p1, p2);
    float a = vec3_dot(d1, d1);
    float e = vec3_dot(d2, d2);
    float f = vec3_dot(d2, r);

    if (a <= EPSILON && e <= EPSILON) {
        *s = *t = 0.0f;
        *c1 = p1;
        *c2 = p2;
        return vec3_dot(vec3_sub(*c1, *c2), vec3_sub(*c1, *c2)); 
    }
    if (a <= EPSILON) {
        *s = 0.0f;
        *t = f / e;
        *t = clamp(*t, 0.0f, 1.0f);
    } else {
        float c = vec3_dot(d1, r);
        if (e <= EPSILON) {
            *t = 0.0f;
            *s = -c / a;
        } else {
            float b = vec3_dot(d1, d2);
            float denom = a*e - b*b;

            if (denom != 0.0f) {
                *s = (b*f - c*e) / denom;
            } else {
                *s = 0.0f;
            }
            *t = (b*(*s) + f) / e;

            if (*t < 0.0f) {
                *t = 0.0f;
                *s = -c / a;
            } else if (*t > 1.0f) {
                *t = 1.0f;
                *s = (b - c) / a;
            }
        }
    }

    *c1 = vec3_add(p1, vec3_scale(d1, *s));
    *c2 = vec3_add(p2, vec3_scale(d2, *t));
    return vec3_dot(vec3_sub(*c1, *c2), vec3_sub(*c1, *c2));
}

float closest_point_ray_ray(vec3 p1, vec3 q1, vec3 p2, vec3 q2, float *s, float *t, vec3 *c1, vec3 *c2) {
    vec3 d1 = vec3_sub(q1, p1);
    vec3 d2 = vec3_sub(q2, p2);
    vec3 r = vec3_sub(p1, p2);
    float a = vec3_dot(d1, d1);
    float e = vec3_dot(d2, d2);
    float f = vec3_dot(d2, r);

    if (a <= EPSILON && e <= EPSILON) {
        *s = *t = 0.0f;
        *c1 = p1;
        *c2 = p2;
        return vec3_dot(vec3_sub(*c1, *c2), vec3_sub(*c1, *c2)); 
    }
    if (a <= EPSILON) {
        *s = 0.0f;
        *t = f / e;
    } else {
        float c = vec3_dot(d1, r);
        if (e <= EPSILON) {
            *t = 0.0f;
            *s = -c / a;
        } else {
            float b = vec3_dot(d1, d2);
            float denom = a*e - b*b;

            if (denom != 0.0f) {
                *s = (b*f - c*e) / denom;
            } else {
                *s = 0.0f;
            }
            *t = (b*(*s) + f) / e;
        }
    }

    *c1 = vec3_add(p1, vec3_scale(d1, *s));
    *c2 = vec3_add(p2, vec3_scale(d2, *t));
    return vec3_dot(vec3_sub(*c1, *c2), vec3_sub(*c1, *c2));
}

vec3 closest_point_ray_circle(vec3 ro, vec3 rd, vec3 circle_center, vec3 circle_normal, float circle_radius) {
    float t;
    int idx;
    if (ray_intersect_planes(ro, rd, &circle_center, &circle_normal, 1, &t, &idx)) {
        vec3 p = vec3_add(ro, vec3_scale(rd, t));    
        return vec3_add(circle_center, vec3_set_length(vec3_subtract(p, circle_center), circle_radius));
    } else {
        // This is wrong
        return circle_center;
    }
}

bool intersection_moving_sphere_triangle(vec3 sc, float sr, vec3 v, vec3 tp0, vec3 tp1, vec3 tp2, float *t, vec3 *p, vec3 *n) {
    vec3 e0 = vec3_sub(tp1, tp0);
    vec3 e1 = vec3_sub(tp2, tp1);
    vec3 pn = vec3_normalize(vec3_cross(e0, e1));
    *n = pn;
    float pd = vec3_dot(tp0, pn);

    float dist = vec3_dot(pn, sc) - pd;
    float denom = vec3_dot(pn, v);
    if (denom * dist >= 0.0f) {
        return false;
    } else {
        float r = dist > 0.0f ? sr : -sr;
        float t0 = (r - dist) / denom;
        vec3 p0 = vec3_add(vec3_add(sc, vec3_scale(v, t0)), vec3_scale(pn, -r));
        enum triangle_contact_type type;
        *p = closest_point_point_triangle(p0, tp0, tp1, tp2, &type);

        int idx;
        return ray_intersect_spheres(*p, vec3_scale(v, -1.0f), &sc, &sr, 1, t, &idx); 
    }
}

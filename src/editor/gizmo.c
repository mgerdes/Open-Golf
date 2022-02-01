#include "editor/gizmo.h"

#include <float.h>

#include "common/graphics.h"
#include "common/inputs.h"

void golf_gizmo_init(golf_gizmo_t *gizmo) {
    gizmo->is_on = false;
    gizmo->is_active = false;
    gizmo->is_hovered = false;
    gizmo->operation = GOLF_GIZMO_TRANSLATE;
    gizmo->mode = GOLF_GIZMO_LOCAL;
    gizmo->translate.snap = 0.25f;
    gizmo->rotate.snap = 22.5f;
    gizmo->scale.snap = 0.25f;
}

static vec2 _world_to_screen(vec3 p) {
    return golf_graphics_world_to_screen(p);
}

static void _add_line(ImDrawList *draw_list, vec2 p0, vec2 p1, float radius, vec4 color) {
    ImVec2 im_p0 = (ImVec2){ p0.x, p0.y };
    ImVec2 im_p1 = (ImVec2){ p1.x, p1.y };
    ImU32 im_col = igGetColorU32_Vec4((ImVec4){ color.x, color.y, color.z, color.w });
    ImDrawList_AddLine(draw_list, im_p0, im_p1, im_col, radius);
}

static void _add_circle(ImDrawList *draw_list, vec2 p, float r, vec4 color) {
    ImVec2 im_p = (ImVec2){ p.x, p.y };
    ImU32 im_col = igGetColorU32_Vec4((ImVec4){ color.x, color.y, color.z, color.w });
    ImDrawList_AddCircleFilled(draw_list, im_p, r, im_col, 100);
}

static void _add_triangle(ImDrawList *draw_list, vec2 p0, vec2 p1, vec2 p2, vec4 color) {
    ImVec2 im_p0 = (ImVec2){ p0.x, p0.y };
    ImVec2 im_p1 = (ImVec2){ p1.x, p1.y };
    ImVec2 im_p2 = (ImVec2){ p2.x, p2.y };
    ImU32 im_col = igGetColorU32_Vec4((ImVec4){ color.x, color.y, color.z, color.w });
    ImDrawList_AddTriangleFilled(draw_list, im_p0, im_p1, im_p2, im_col);
}

static void _add_convex_poly(ImDrawList *draw_list, vec2 *points, int num_points, vec4 color) {
    ImU32 im_col = igGetColorU32_Vec4((ImVec4){ color.x, color.y, color.z, color.w });
    ImDrawList_AddConvexPolyFilled(draw_list, (ImVec2*)points, num_points, im_col);
}

void golf_gizmo_update(golf_gizmo_t *gizmo, ImDrawList *draw_list) {
    if (!gizmo->is_on) {
        return;
    }

    golf_inputs_t *inputs = golf_inputs_get();
    golf_graphics_t *graphics = golf_graphics_get();

    vec3 p = gizmo->transform.position;
    float dist_modifier = 0.1f * vec3_distance(graphics->cam_pos, p);

    mat4 model_mat = mat4_from_quat(gizmo->transform.rotation);
    vec3 axis[3] = {
        vec3_apply_mat4(V3(1, 0, 0), 0, model_mat),
        vec3_apply_mat4(V3(0, 1, 0), 0, model_mat),
        vec3_apply_mat4(V3(0, 0, 1), 0, model_mat),
    };
    vec4 color[3] = { 
        V4(1, 0, 0, 1),
        V4(0, 1, 0, 1),
        V4(0, 0, 1, 1) 
    };

    if (gizmo->operation == GOLF_GIZMO_TRANSLATE) {
        int closest_axis = -1;
        float closest_dist = FLT_MAX;
        vec3 closest_point[3];
        for (int i = 0; i < 3; i++) {
            vec3 p1 = inputs->mouse_ray_orig;
            vec3 q1 = vec3_add(p1, inputs->mouse_ray_dir);
            vec3 p2 = p;
            vec3 q2 = vec3_add(p2, vec3_scale(axis[i], dist_modifier));
            float s, t;
            vec3 c1, c2;
            closest_point_ray_ray(p1, q1, p2, q2, &s, &t, &c1, &c2);
            float screen_dist = vec2_distance(_world_to_screen(c1), _world_to_screen(c2));
            if (t >= 0 && t <= 1.4f && screen_dist < closest_dist) {
                closest_dist = screen_dist;
                closest_axis = i;
            }
            closest_point[i] = c2;
        }

        int hovered_axis = -1;
        if (closest_dist < 8) {
            hovered_axis = closest_axis;
        }

        if (gizmo->is_active) {
            igCaptureMouseFromApp(true);
            if (!inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
                gizmo->is_active = false;
            }

            int axis_i = gizmo->translate.axis;
            vec3 translate = vec3_sub(closest_point[axis_i], gizmo->translate.start_point);
            float dist = golf_snapf(vec3_length(translate), gizmo->translate.snap);
            translate = vec3_set_length(translate, dist);
            gizmo->transform.position = vec3_add(gizmo->translate.start_position, translate);
            gizmo->delta_transform.position = translate;
        }
        else {
            if (hovered_axis >= 0 && inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
                gizmo->is_active = true;
                gizmo->translate.axis = hovered_axis;
                gizmo->translate.start_point = closest_point[hovered_axis];
                gizmo->translate.start_position = gizmo->transform.position;
                gizmo->delta_transform = golf_transform(V3(0, 0, 0), V3(0, 0, 0), QUAT(0, 0, 0, 1));
            }
        }

        for (int i = 0; i < 3; i++) {
            vec2 p0 = _world_to_screen(p);
            vec2 p1 = _world_to_screen(vec3_add(p, vec3_scale(axis[i], dist_modifier)));
            float thickness = 4;
            vec4 col = color[i];
            if ((!gizmo->is_active && hovered_axis == i) ||
                    (gizmo->is_active && i == gizmo->translate.axis)){
                col = V4(1, 1, 0, 1);
            }
            _add_line(draw_list, p0, p1, thickness, col);

            float w = 15;
            float h = sqrtf(3.0f) / 2.0f * w;
            vec2 dir0 = vec2_normalize(vec2_sub(p1, p0));
            vec2 dir1 = vec2_rotate(dir0, 0.5f * MF_PI);
            vec2 t_p0 = vec2_add(p1, vec2_add(vec2_scale(dir0, -0.5f * h), vec2_scale(dir1, -0.5f * w)));
            vec2 t_p1 = vec2_add(p1, vec2_add(vec2_scale(dir0, -0.5f * h), vec2_scale(dir1, 0.5f * w)));
            vec2 t_p2 = vec2_add(p1, vec2_scale(dir0, 0.5f * h));
            _add_triangle(draw_list, t_p0, t_p1, t_p2, col);
        }
    }
    else if (gizmo->operation == GOLF_GIZMO_ROTATE) {
        vec3 ro = inputs->mouse_ray_orig;
        vec3 rd = inputs->mouse_ray_dir;
        vec3 cc = p;
        float cr = 1 * dist_modifier;

        vec3 circle_p0[3], circle_p[3];
        int closest_axis = -1;
        float closest_dist = FLT_MAX;
        for (int i = 0; i < 3; i++) {
            circle_p0[i] = closest_point_point_plane(ro, cc, axis[i]);
            circle_p0[i] = vec3_add(cc, vec3_set_length(vec3_sub(circle_p0[i], cc), cr));

            float t;
            int idx;
            if (!ray_intersect_planes(ro, rd, &cc, &axis[i], 1, &t, &idx)) {
                continue;
            }

            vec3 plane_p = vec3_add(ro, vec3_scale(rd, t));
            circle_p[i] = vec3_add(cc, vec3_set_length(vec3_sub(plane_p, cc), cr));

            quat r = quat_between_vectors(vec3_sub(circle_p0[i], cc), vec3_sub(circle_p[i], cc));
            vec3 r_axis;
            float r_angle;
            quat_get_axis_angle(r, &r_axis, &r_angle);
            if (r_angle > 0.5f * MF_PI || r_angle < -0.5f * MF_PI) {
                continue;
            }

            float screen_dist = vec2_distance(_world_to_screen(plane_p), _world_to_screen(circle_p[i]));
            if (screen_dist < closest_dist) {
                closest_axis = i;
                closest_dist = screen_dist;
            }
        }

        float hover_dist = 8;
        int hovered_axis = -1;
        if (closest_dist < hover_dist) {
            hovered_axis = closest_axis;
        }

        if (gizmo->is_active) {
            igCaptureMouseFromApp(true);
            if (!inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
                gizmo->is_active = false;
            }

            int axis_i = gizmo->rotate.axis;
            vec3 dir0 = vec3_normalize(vec3_sub(gizmo->rotate.start_point, cc));
            vec3 dir1 = vec3_normalize(vec3_sub(circle_p[axis_i], cc));
            quat r = quat_between_vectors(dir0, dir1);
            vec3 r_axis;
            float r_angle;
            quat_get_axis_angle(r, &r_axis, &r_angle);

            if (r_angle != 0) {
                if (vec3_dot(r_axis, gizmo->rotate.axis_rotation) < 0.0f) {
                    r_angle *= -1.0f;
                }
                r_angle = golf_snapf(r_angle, gizmo->rotate.snap * (MF_PI / 180.0f));
                quat rotation = quat_create_from_axis_angle(gizmo->rotate.axis_rotation, r_angle);
                gizmo->transform.rotation = quat_multiply(rotation, gizmo->rotate.start_rotation);
                gizmo->delta_transform.rotation = rotation;
            }

            int num_points = 25;
            vec2 points[25];
            points[0] = _world_to_screen(cc);
            for (int i = 1; i < num_points; i++) {
                float t = r_angle * ((float)(i - 1) / (num_points - 2));
                vec3 p = vec3_add(cc, vec3_scale(vec3_rotate_about_axis(dir0, gizmo->rotate.axis_rotation, t), cr));
                points[i] = _world_to_screen(p);
            }
            vec4 col = V4(1, 1, 1, 0.4f);
            _add_convex_poly(draw_list, points, num_points, col);
        }
        else {
            if (hovered_axis >= 0 && inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
                gizmo->is_active = true;
                gizmo->rotate.axis = hovered_axis;
                gizmo->rotate.axis_rotation = axis[hovered_axis];
                gizmo->rotate.start_point = circle_p[hovered_axis];
                gizmo->rotate.start_rotation = gizmo->transform.rotation;
                gizmo->delta_transform = golf_transform(V3(0, 0, 0), V3(0, 0, 0), QUAT(0, 0, 0, 1));
            }
        }

        for (int i = 0; i < 3; i++) {
            vec4 col = color[i];
            if ((!gizmo->is_active && i == hovered_axis) ||
                    (gizmo->is_active && i == gizmo->rotate.axis)) {
                col = V4(1, 1, 0, 1);
            }
            float thickness = 4;
            int n = 20;
            vec3 pt = circle_p0[i];
            for (int j = 0; j < n; j++) {
                float t0 = 0.5f * MF_PI * ((float) j / n);
                float t1 = 0.5f * MF_PI * ((float) (j + 1) / n);
                vec3 dir = vec3_sub(pt, cc);
                vec2 p0 = _world_to_screen(vec3_add(cc, vec3_rotate_about_axis(dir, axis[i], t0)));
                vec2 p1 = _world_to_screen(vec3_add(cc, vec3_rotate_about_axis(dir, axis[i], t1)));
                _add_line(draw_list, p0, p1, thickness, col); 

                p0 = _world_to_screen(vec3_add(cc, vec3_rotate_about_axis(dir, axis[i], -t0)));
                p1 = _world_to_screen(vec3_add(cc, vec3_rotate_about_axis(dir, axis[i], -t1)));
                _add_line(draw_list, p0, p1, thickness, col); 
            }
        }
    }
    else if (gizmo->operation == GOLF_GIZMO_SCALE) {
        int closest_axis = -1;
        float closest_dist = FLT_MAX;
        vec3 closest_point[3];
        for (int i = 0; i < 3; i++) {
            vec3 p1 = inputs->mouse_ray_orig;
            vec3 q1 = vec3_add(p1, inputs->mouse_ray_dir);
            vec3 p2 = p;
            vec3 q2 = vec3_add(p2, vec3_scale(axis[i], dist_modifier));
            float s, t;
            vec3 c1, c2;
            closest_point_ray_ray(p1, q1, p2, q2, &s, &t, &c1, &c2);
            float screen_dist = vec2_distance(_world_to_screen(c1), _world_to_screen(c2));
            if (t >= 0 && t <= 1.4f && screen_dist < closest_dist) {
                closest_dist = screen_dist;
                closest_axis = i;
            }
            closest_point[i] = c2;
        }

        int hovered_axis = -1;
        if (closest_dist < 8) {
            hovered_axis = closest_axis;
        }

        if (gizmo->is_active) {
            igCaptureMouseFromApp(true);
            if (!inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
                gizmo->is_active = false;
            }

            int axis_i = gizmo->scale.axis;
            vec3 scale = vec3_sub(closest_point[axis_i], gizmo->scale.start_point);
            scale = vec3_snap(scale, gizmo->scale.snap);
            gizmo->transform.scale = vec3_add(gizmo->scale.start_scale, scale);
            gizmo->delta_transform.scale = scale;
        }
        else {
            if (hovered_axis >= 0 && inputs->mouse_down[SAPP_MOUSEBUTTON_LEFT]) {
                gizmo->is_active = true;
                gizmo->scale.axis = hovered_axis;
                gizmo->scale.start_point = closest_point[hovered_axis];
                gizmo->scale.start_scale = gizmo->transform.scale;
                gizmo->delta_transform = golf_transform(V3(0, 0, 0), V3(0, 0, 0), QUAT(0, 0, 0, 1));
            }
        }

        for (int i = 0; i < 3; i++) {
            vec2 p0 = _world_to_screen(p);
            vec2 p1 = _world_to_screen(vec3_add(p, vec3_scale(axis[i], dist_modifier)));
            float thickness = 4;
            vec4 col = color[i];
            if ((!gizmo->is_active && hovered_axis == i) ||
                    (gizmo->is_active && i == gizmo->scale.axis)){
                col = V4(1, 1, 0, 1);
            }
            _add_line(draw_list, p0, p1, thickness, col);

            float r = 7.5;
            _add_circle(draw_list, p1, r, col);
        }
    }
}

void golf_gizmo_set_operation(golf_gizmo_t *gizmo, golf_gizmo_operation operation) {
    if (gizmo->is_active) {
        return;
    }

    gizmo->operation = operation;
}

void golf_gizmo_set_mode(golf_gizmo_t *gizmo, golf_gizmo_mode mode) {
    if (gizmo->is_active) {
        return;
    }

    gizmo->mode = mode;
}

void golf_gizmo_set_transform(golf_gizmo_t *gizmo, golf_transform_t transform) {
    if (gizmo->is_active) {
        return;
    }

    gizmo->transform = transform;
}

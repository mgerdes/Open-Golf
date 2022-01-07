#include "golf/gizmo.h"

#include "golf/inputs.h"
#include "golf/renderer.h"

void golf_gizmo_init(golf_gizmo_t *gizmo) {
    gizmo->is_on = false;
    gizmo->is_active = false;
    gizmo->is_hovered = false;
    gizmo->operation = GOLF_GIZMO_TRANSLATE;
    gizmo->transform = NULL;
}

static vec2 _world_to_screen(vec3 p) {
    return golf_renderer_world_to_screen(p);
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

static void _add_axis_tip(ImDrawList *draw_list, vec2 p0, vec2 p1, float w, vec4 color) {
    float h = sqrtf(3.0f) / 2.0f * w;
    vec2 dir0 = vec2_normalize(vec2_sub(p1, p0));
    vec2 dir1 = vec2_rotate(dir0, 0.5f * MF_PI);
    vec2 t_p0 = vec2_add(p1, vec2_scale(dir1, -0.5f * w));
    vec2 t_p1 = vec2_add(p1, vec2_scale(dir1, 0.5f * w));
    vec2 t_p2 = vec2_add(p1, vec2_scale(dir0, h));
    _add_triangle(draw_list, t_p0, t_p1, t_p2, color);
}

static void _add_rotation_semi_circle(ImDrawList *draw_list, vec3 p, float r, vec3 axis, float thickness, vec4 color, int n) {
    golf_renderer_t *renderer = golf_renderer_get();
    vec3 pt = closest_point_point_circle(renderer->cam_pos, p, axis, r);
    for (int i = 0; i < n; i++) {
        float t0 = 0.5f * MF_PI * ((float) i / n);
        float t1 = 0.5f * MF_PI * ((float) (i + 1) / n);
        vec3 dir = vec3_sub(pt, p);
        vec2 p0 = _world_to_screen(vec3_add(p, vec3_rotate_about_axis(dir, axis, t0)));
        vec2 p1 = _world_to_screen(vec3_add(p, vec3_rotate_about_axis(dir, axis, t1)));
        _add_line(draw_list, p0, p1, thickness, color); 

        p0 = _world_to_screen(vec3_add(p, vec3_rotate_about_axis(dir, axis, -t0)));
        p1 = _world_to_screen(vec3_add(p, vec3_rotate_about_axis(dir, axis, -t1)));
        _add_line(draw_list, p0, p1, thickness, color); 
    }
}

void golf_gizmo_update(golf_gizmo_t *gizmo, ImDrawList *draw_list) {
    if (!gizmo->is_on || !gizmo->transform) {
        return;
    }

    golf_inputs_t *inputs = golf_inputs_get();

    golf_config_t *cfg = golf_data_get_config("data/config/editor.cfg");
    vec3 p = gizmo->transform->position;
    vec2 p_screen = _world_to_screen(p);

    vec2 x_axis = _world_to_screen(vec3_add(p, V3(1, 0, 0)));
    _add_line(draw_list, p_screen, x_axis, CFG_NUM(cfg, "gizmo_line_thickness"), CFG_VEC4(cfg, "gizmo_x_axis_color"));
    _add_axis_tip(draw_list, p_screen, x_axis, CFG_NUM(cfg, "gizmo_axis_tip_width"), CFG_VEC4(cfg, "gizmo_x_axis_color"));

    vec2 y_axis = _world_to_screen(vec3_add(p, V3(0, 1, 0)));
    _add_line(draw_list, p_screen, y_axis, CFG_NUM(cfg, "gizmo_line_thickness"), CFG_VEC4(cfg, "gizmo_y_axis_color"));
    _add_axis_tip(draw_list, p_screen, y_axis, CFG_NUM(cfg, "gizmo_axis_tip_width"), CFG_VEC4(cfg, "gizmo_y_axis_color"));

    vec2 z_axis = _world_to_screen(vec3_add(p, V3(0, 0, 1)));
    _add_line(draw_list, p_screen, z_axis, CFG_NUM(cfg, "gizmo_line_thickness"), CFG_VEC4(cfg, "gizmo_z_axis_color"));
    _add_axis_tip(draw_list, p_screen, z_axis, CFG_NUM(cfg, "gizmo_axis_tip_width"), CFG_VEC4(cfg, "gizmo_z_axis_color"));

    {
        vec3 ro = inputs->mouse_ray_orig;
        vec3 rd = inputs->mouse_ray_dir;
        vec3 circle_center = p;
        vec3 circle_normal = V3(1, 0, 0);
        float t;
        int idx;
        if (ray_intersect_planes(inputs->mouse_ray_orig, inputs->mouse_ray_dir, &circle_center, &circle_normal, 1, &t, &idx)) {
            float radius = CFG_NUM(cfg, "gizmo_rotation_circle_radius");  
            vec3 p = vec3_add(ro, vec3_scale(rd, t));
            float dist = vec3_distance(p, circle_center);
            dist = fabsf(dist - radius);
        }
    }

    _add_rotation_semi_circle(draw_list, 
            p, 
            CFG_NUM(cfg, "gizmo_rotation_circle_radius"), 
            V3(1, 0, 0), 
            CFG_NUM(cfg, "gizmo_line_thickness"),
            CFG_VEC4(cfg, "gizmo_x_axis_color"), 
            CFG_NUM(cfg, "gizmo_rotation_circle_segments"));
    _add_rotation_semi_circle(draw_list, 
            p, 
            CFG_NUM(cfg, "gizmo_rotation_circle_radius"), 
            V3(0, 1, 0), 
            CFG_NUM(cfg, "gizmo_line_thickness"),
            CFG_VEC4(cfg, "gizmo_y_axis_color"), 
            CFG_NUM(cfg, "gizmo_rotation_circle_segments"));
    _add_rotation_semi_circle(draw_list, 
            p, 
            CFG_NUM(cfg, "gizmo_rotation_circle_radius"), 
            V3(0, 0, 1), 
            CFG_NUM(cfg, "gizmo_line_thickness"),
            CFG_VEC4(cfg, "gizmo_z_axis_color"), 
            CFG_NUM(cfg, "gizmo_rotation_circle_segments"));


    _add_circle(draw_list, p_screen, CFG_NUM(cfg, "gizmo_point_radius"), CFG_VEC4(cfg, "gizmo_point_color"));
}

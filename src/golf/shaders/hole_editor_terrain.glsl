@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs hole_editor_terrain_vs
uniform hole_editor_terrain_vs_params {
    mat4 proj_view_mat;
    mat4 model_mat;
    vec4 color0[5];
    vec4 color1[5];
};

in vec3 position;
in vec3 normal;
in vec2 texture_coord;
in vec2 lightmap_uv;
in float material_idx;

out vec3 frag_normal;
out vec2 frag_texture_coord;
out vec2 frag_lightmap_uv;
out vec3 frag_color0;
out vec3 frag_color1;
out float frag_material_idx;

void main() {
    frag_normal = (transpose(inverse(model_mat)) * vec4(normal, 0.0)).xyz;
    frag_normal = normalize(frag_normal);
    frag_texture_coord = texture_coord;
    frag_lightmap_uv = lightmap_uv;
    frag_color0 = color0[int(material_idx)].xyz;
    frag_color1 = color1[int(material_idx)].xyz;
    frag_material_idx = material_idx;
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
@end

@fs hole_editor_terrain_fs
uniform hole_editor_terrain_fs_params {
    float draw_type;
    vec4 uniform_color;
    float lightmap_t;
};
uniform sampler2D ce_lightmap_tex0, ce_lightmap_tex1;
uniform sampler2D ce_tex0, ce_tex1;

in vec3 frag_normal;
in vec2 frag_texture_coord;
in vec2 frag_lightmap_uv;
in vec3 frag_color0;
in vec3 frag_color1;
in float frag_material_idx;

out vec4 g_frag_color;

void main() {
    vec3 light_dir[16];
    light_dir[0] = vec3(0.521705, -0.818730, 0.239803);
    light_dir[1] = vec3(-0.640216, -0.699187, 0.318216);
    light_dir[2] = vec3(-0.153738, -0.982361, 0.106449);
    light_dir[3] = vec3(0.739134, -0.423210, 0.523999);
    light_dir[4] = vec3(0.342038, -0.902011, -0.263413);
    light_dir[5] = vec3(-0.321816, -0.422090, 0.847511);
    light_dir[6] = vec3(0.303894, -0.945710, 0.115244);
    light_dir[7] = vec3(-0.273859, -0.896177, 0.349096);
    light_dir[8] = vec3(-0.647848, -0.654808, 0.389255);
    light_dir[9] = vec3(0.109161, -0.762333, 0.637913);
    light_dir[10] = vec3(0.226783, -0.294490, 0.928356);
    light_dir[11] = vec3(0.016693, -0.992646, -0.119893);
    light_dir[12] = vec3(-0.449255, -0.491488, -0.746063);
    light_dir[13] = vec3(0.161727, -0.196726, -0.967028);
    light_dir[14] = vec3(0.633646, -0.773218, 0.025036);
    light_dir[15] = vec3(0.624673, -0.720306, 0.301566);

    int x = int(frag_texture_coord.x + 0.00001 * frag_lightmap_uv.x);
    int y = int(frag_texture_coord.y + 0.00001 * frag_lightmap_uv.y);
    vec3 color;
    if ((x + y) % 2 == 0) {
        color = frag_color0;
    }
    else {
        color = frag_color1;
    }

    if (round(frag_material_idx) == 0) {
        color = texture(ce_tex0, frag_texture_coord).xyz;
    }
    else if (round(frag_material_idx) == 1) {
        color = texture(ce_tex1, frag_texture_coord).xyz;
    }

    float kd = 0.0;
    for (int i = 0; i < 16; i++) {
        kd += max(dot(normalize(frag_normal), -light_dir[i]), 0.4);
    }
    kd *= (1.0 / 16.0);
    kd += 0.3f;

    float ao0 = texture(ce_lightmap_tex0, frag_lightmap_uv).r;
    float ao1 = texture(ce_lightmap_tex1, frag_lightmap_uv).r;
    float ao = ao0 + (ao1 - ao0) * lightmap_t;

    if (int(draw_type) == 0) {
        g_frag_color = vec4(ao * color, 1.0);
    }
    else if (int(draw_type) == 1) {
        g_frag_color = vec4(kd * color, 1.0);
    }
    else if (int(draw_type) == 2) {
        g_frag_color = vec4(ao, ao, ao, 1.0);
    }
    else if (int(draw_type) == 3) {
        g_frag_color = vec4(frag_lightmap_uv, 0.0, 1.0);
    }
    else if (int(draw_type) < 7) {
        g_frag_color = uniform_color;
    }
}
@end

@program hole_editor_terrain hole_editor_terrain_vs hole_editor_terrain_fs

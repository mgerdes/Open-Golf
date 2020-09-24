@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs hole_editor_ground_vs
uniform hole_editor_ground_vs_params {
    mat4 proj_view_mat;
    mat4 model_mat;
};

in vec3 position;
in vec3 normal;
in vec2 texture_coord;
in vec2 lightmap_uv;

out vec3 frag_normal;
out vec2 frag_texture_coord;
out vec2 frag_lightmap_uv;

void main() {
    frag_normal = (transpose(inverse(model_mat)) * vec4(normal, 0.0)).xyz;
    frag_normal = normalize(frag_normal);
    frag_texture_coord = texture_coord;
    frag_lightmap_uv = lightmap_uv;
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
@end

@fs hole_editor_ground_fs
uniform hole_editor_ground_fs_params {
    float draw_type;
};
uniform sampler2D ce_lightmap_tex, ce_material_tex, ce_perlin_noise_tex;

in vec3 frag_normal;
in vec2 frag_texture_coord;
in vec2 frag_lightmap_uv;

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

    float kd = 0.0;
    for (int i = 0; i < 16; i++) {
        kd += max(dot(normalize(frag_normal), -light_dir[i]), 0.4);
    }
    kd *= (1.0 / 16.0);
    kd += 0.1f;


    float ao = texture(ce_lightmap_tex, frag_lightmap_uv).r;
    vec2 tc_delta = mod(frag_texture_coord, 0.2);
    if (tc_delta.x >= 0.1) tc_delta.x = 0.2 - tc_delta.x;
    if (tc_delta.y >= 0.1) tc_delta.y = 0.2 - tc_delta.y;
    vec2 material_tc0 = vec2(0.39, 0.1) + tc_delta;
    vec3 color0 = texture(ce_material_tex, material_tc0).xyz;
    vec2 material_tc1 = vec2(0.69, 0.1) + tc_delta;
    vec3 color1 = texture(ce_material_tex, material_tc1).xyz;

    float perlin_noise = texture(ce_perlin_noise_tex, frag_texture_coord).x;
    vec3 color;
    if (perlin_noise > 0.45) {
        color = color1;
    }
    else if (perlin_noise < 0.35) {
        color = color0;
    }
    else {
        float a = (perlin_noise - 0.35) / (0.45 - 0.35);
        color = a * color1 + (1 - a) * color0;
    }
    color.x -= 0.1 * perlin_noise;
    color.y -= 0.1 * perlin_noise;
    color.z -= 0.1 * perlin_noise;

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
        g_frag_color.x += 0.00001 * frag_texture_coord.x;
    }
}
@end

@program hole_editor_ground hole_editor_ground_vs hole_editor_ground_fs


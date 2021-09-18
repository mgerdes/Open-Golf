@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs single_color_vs
uniform single_color_vs_params {
    mat4 model_mat;
    mat4 proj_view_mat;
};

in vec3 position;
in vec3 normal;

out vec3 frag_normal;

void main() {
    frag_normal = normalize((transpose(inverse(model_mat)) * vec4(normal, 0.0)).xyz);
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
@end

@fs single_color_fs
uniform single_color_fs_params {
    vec4 color;
    float kd_scale;
};

in vec3 frag_normal;

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
        kd += max(dot(normalize(frag_normal), -light_dir[i]), 0.1);
    }
    kd *= (1.0 / 16.0);

    vec3 c = kd_scale * kd * color.xyz + (1.0 - kd_scale) * color.xyz;
    g_frag_color = vec4(c, color.w);
}
@end

@program single_color single_color_vs single_color_fs 

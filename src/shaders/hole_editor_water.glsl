@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs hole_editor_water_vs
uniform hole_editor_water_vs_params {
    mat4 proj_view_mat;
    mat4 model_mat;
};

in vec3 position;
in vec2 texture_coord;

out vec2 frag_texture_coord;

void main() {
    frag_texture_coord = texture_coord;
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
@end

@fs hole_editor_water_fs
uniform hole_editor_water_fs_params {
    float t;
};
uniform sampler2D ce_water_noise_tex0, ce_water_noise_tex1;

in vec2 frag_texture_coord;

out vec4 g_frag_color;

void main() {
    vec2 tc0 = vec2(frag_texture_coord.x, 0.1 * (frag_texture_coord.y + 16 * t));
    vec2 tc1 = vec2(frag_texture_coord.x, frag_texture_coord.y + t);

    float noise0 = texture(ce_water_noise_tex1, tc0).w;
    float noise1 = texture(ce_water_noise_tex0, tc1).w;

    vec3 color = vec3(1.0/255.0, 124.0/255.0, 143.0/255.0);
    if (noise1 > 0.1) {
        color = vec3(0.6);
    }
    if (noise0 > 0.1) {
        color = vec3(0.8);
    } 
    if (frag_texture_coord.x > 0.95) {
        float a = (frag_texture_coord.x - 0.95) / 0.05;
        color = a * vec3(1.0) + (1.0 - a) * color;
    }
    if (frag_texture_coord.x < 0.05) {
        float a = (0.05 - frag_texture_coord.x) / 0.05;
        color = a * vec3(1.0) + (1.0 - a) * color;
    }
    g_frag_color = vec4(color, 0.85);
}
@end

@program hole_editor_water hole_editor_water_vs hole_editor_water_fs

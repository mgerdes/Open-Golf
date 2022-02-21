@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs water_vs
uniform water_vs_params {
    mat4 proj_view_mat;
    mat4 model_mat;
};

in vec3 position;
in vec2 texture_coord;
in vec2 lightmap_uv;

out vec2 frag_texture_coord;
out vec2 frag_lightmap_uv;

void main() {
    frag_texture_coord = texture_coord;
    frag_lightmap_uv = lightmap_uv;
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
@end

@fs water_fs
uniform water_fs_params {
    float t;
};
uniform sampler2D water_lightmap_tex;
uniform sampler2D water_noise_tex0, water_noise_tex1;

in vec2 frag_texture_coord;
in vec2 frag_lightmap_uv;

out vec4 g_frag_color;

void main() {
    vec2 tc0 = vec2(frag_texture_coord.x, 0.1 * (frag_texture_coord.y + 0.8 * t));
    vec2 tc1 = vec2(frag_texture_coord.x, frag_texture_coord.y + 0.0625 * t);

    float noise0 = texture(water_noise_tex1, tc0).w;
    float noise1 = texture(water_noise_tex0, tc1).w;

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

    float ao = texture(water_lightmap_tex, frag_lightmap_uv).r;

    g_frag_color = vec4(ao*color, 0.85);
}
@end

@program water water_vs water_fs

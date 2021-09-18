@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs environment_vs
uniform environment_vs_params {
    mat4 model_mat;
    mat4 proj_view_mat;
};

in vec3 position;
in vec2 texture_coord;
in vec2 lightmap_uv;

out vec3 frag_position;
out vec2 frag_texture_coord;
out vec2 frag_lightmap_uv;

void main() {
    frag_position = (model_mat * vec4(position, 1.0)).xyz;
    frag_texture_coord = texture_coord;
    frag_lightmap_uv = lightmap_uv;
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
@end

@fs environment_fs
uniform environment_fs_params {
    vec4 ball_position;
};
uniform sampler2D environment_lightmap_tex;
uniform sampler2D environment_material_tex;

in vec3 frag_position;
in vec2 frag_texture_coord;
in vec2 frag_lightmap_uv;

out vec4 g_frag_color;

void main() {
    float ao = texture(environment_lightmap_tex, frag_lightmap_uv).r;
    float dist_to_ball = distance(frag_position.xz, ball_position.xz);
    if (dist_to_ball < 0.14 && frag_position.y < ball_position.y) {
        float scale = (0.14 - dist_to_ball) / 0.14;
        ao -= scale * scale * 0.8;
    }
    vec3 color = texture(environment_material_tex, frag_texture_coord).xyz;
    g_frag_color = vec4(ao * color, 1.0);
}
@end

@program environment environment_vs environment_fs

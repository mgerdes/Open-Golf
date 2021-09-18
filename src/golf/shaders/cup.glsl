@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs cup_vs
uniform cup_vs_params {
    mat4 proj_view_mat;
    mat4 model_mat;
};

in vec3 position;
in vec2 lightmap_uv;

out vec2 frag_lightmap_uv;

void main() {
    frag_lightmap_uv = lightmap_uv;
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
@end

@fs cup_fs
uniform sampler2D lightmap_tex;

in vec2 frag_lightmap_uv;

out vec4 g_frag_color;

void main() {
    float ao = texture(lightmap_tex, frag_lightmap_uv).r;
    g_frag_color = vec4(ao, ao, ao, 1.0);
}
@end

@program cup cup_vs cup_fs


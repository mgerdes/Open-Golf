@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs ui_single_color_vs
uniform ui_single_color_vs_params {
    mat4 mvp_mat;
};

in vec3 position;

out vec2 frag_texture_coord;

void main() {
    gl_Position = mvp_mat*vec4(position, 1.0);
}
@end

@fs ui_single_color_fs
uniform ui_single_color_fs_params {
    vec4 color;
};

in vec2 frag_texture_coord;

out vec4 g_frag_color;

void main() {
    g_frag_color = color;
}
@end

@program ui_single_color ui_single_color_vs ui_single_color_fs

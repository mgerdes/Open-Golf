@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs solid_color_material_vs
uniform solid_color_material_vs_params {
    mat4 mvp_mat;
};

in vec3 position;

void main() {
    gl_Position = mvp_mat * vec4(position, 1.0);
}
@end

@fs solid_color_material_fs
uniform solid_color_material_fs_params {
    vec4 color;
};

out vec4 g_frag_color;

void main() {
    g_frag_color = vec4(color);
}
@end

@program solid_color_material solid_color_material_vs solid_color_material_fs

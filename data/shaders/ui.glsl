@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs ui_vs
uniform ui_vs_params {
    mat4 mvp_mat;
};

in vec3 position;
in vec2 texture_coord;

out vec2 frag_texture_coord;

void main() {
    frag_texture_coord = texture_coord;
    gl_Position = mvp_mat * vec4(position, 1.0);
}
@end

@fs ui_fs
uniform ui_fs_params {
    vec4 color;
    float tex_x, tex_y, tex_dx, tex_dy, is_font;
};
uniform sampler2D ui_texture;

in vec2 frag_texture_coord;

out vec4 g_frag_color;

void main() {
    vec2 tc = vec2(tex_x, tex_y) + frag_texture_coord * vec2(tex_dx, tex_dy);
    g_frag_color = texture(ui_texture, tc);
    g_frag_color.a = is_font*g_frag_color.x + (1.0 - is_font)*g_frag_color.a;
    g_frag_color.xyz = (1.0 - is_font)*((1.0 - color.a)*g_frag_color.xyz + color.a*color.xyz) + is_font*color.xyz;
}
@end

@program ui ui_vs ui_fs

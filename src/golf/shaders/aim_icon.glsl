@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs aim_icon_vs
uniform aim_icon_vs_params {
    mat4 mvp_mat;
};

in vec3 position;
in float alpha;

out float frag_alpha;

void main() {
    frag_alpha = alpha;
    gl_Position = mvp_mat * vec4(position, 1.0);
}
@end

@fs aim_icon_fs
uniform aim_icon_fs_params {
    vec4 color;
};

in float frag_alpha;

out vec4 g_frag_color;

void main() {
    g_frag_color = vec4(color.xyz, color.w*frag_alpha);
}
@end

@program aim_icon aim_icon_vs aim_icon_fs

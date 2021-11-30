@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs pass_through_vs
uniform pass_through_vs_params {
    mat4 mvp_mat;
};

in vec3 position;

void main() {
    gl_Position = mvp_mat * vec4(position, 1.0);
}
@end

@fs pass_through_fs
out vec4 g_frag_color;

void main() {
    g_frag_color = vec4(1.0, 1.0, 1.0, 1.0);
}
@end

@program pass_through pass_through_vs pass_through_fs

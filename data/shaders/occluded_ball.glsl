@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs occluded_ball_vs
uniform occluded_ball_vs_params {
    mat4 model_mat;
    mat4 proj_view_mat;
};

in vec3 position;
in vec3 normal;

out vec3 frag_normal;

void main() {
    frag_normal = normalize((inverse(transpose(model_mat))*vec4(normal, 0.0f)).xyz);
    gl_Position = proj_view_mat*model_mat*vec4(position, 1.0);
}
@end

@fs occluded_ball_fs
uniform occluded_ball_fs_params {
    vec4 ball_position;
    vec4 cam_position;
};

in vec3 frag_normal;

out vec4 g_frag_color;

void main() {
    vec3 dir = normalize(cam_position.xyz - ball_position.xyz);
    float d = min(max(dot(frag_normal, dir), 0.0f), 1.0);
    float c = 0.2 + 0.5*d;
    float a = 0.1 + 0.5*(1.0 - d);
    g_frag_color = vec4(c, c, c, a);
}
@end

@program occluded_ball occluded_ball_vs occluded_ball_fs

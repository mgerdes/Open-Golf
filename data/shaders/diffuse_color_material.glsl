@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs diffuse_color_material_vs
uniform diffuse_color_material_vs_params {
    mat4 proj_view_mat;
    mat4 model_mat;
};

in vec3 position;
in vec3 normal;

out vec3 frag_normal;

void main() {
    frag_normal = normalize((transpose(inverse(model_mat)) * vec4(normal, 0.0)).xyz);
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
@end

@fs diffuse_color_material_fs
uniform diffuse_color_material_fs_params {
    vec3 color;
};

in vec3 frag_normal;

out vec4 g_frag_color;

void main() {
    float kd = max(dot(frag_normal, normalize(vec3(1, 1, 1))), 0);
    g_frag_color = vec4(kd * color, 1.0);
}
@end

@program diffuse_color_material diffuse_color_material_vs diffuse_color_material_fs

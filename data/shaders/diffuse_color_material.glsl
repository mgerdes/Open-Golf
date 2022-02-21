//@begin_vert
#version 450

layout(binding = 0) uniform diffuse_color_material_vs_params {
    mat4 proj_view_mat;
    mat4 model_mat;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

layout(location = 0) out vec3 frag_normal;

void main() {
    frag_normal = normalize((transpose(inverse(model_mat)) * vec4(normal, 0.0)).xyz);
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
//@end

//@begin_frag
#version 450

layout(binding = 0) uniform diffuse_color_material_fs_params {
    vec3 color;
};

layout(location = 0) in vec3 frag_normal;

layout(location = 0) out vec4 g_frag_color;

void main() {
    float kd = max(dot(frag_normal, normalize(vec3(1, 1, 1))), 0);
    g_frag_color = vec4(kd * color, 1.0);
}
//@end

//@begin_vert
#version 450

layout(binding = 0) uniform vs_params {
    mat4 model_mat;
    mat4 proj_view_mat;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

layout(location = 0) out vec3 frag_normal;

void main() {
    frag_normal = normalize((inverse(transpose(model_mat))*vec4(normal, 0.0f)).xyz);
    gl_Position = proj_view_mat*model_mat*vec4(position, 1.0);
}
//@end

//@begin_frag
#version 450

layout(binding = 0) uniform fs_params {
    vec4 ball_position;
    vec4 cam_position;
};

layout(location = 0) in vec3 frag_normal;

layout(location = 0) out vec4 g_frag_color;

void main() {
    vec3 dir = normalize(cam_position.xyz - ball_position.xyz);
    float d = min(max(dot(frag_normal, dir), 0.0f), 1.0);
    float c = 0.2 + 0.5*d;
    float a = 0.1 + 0.5*(1.0 - d);
    g_frag_color = vec4(c, c, c, a);
}
//@end

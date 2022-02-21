//@begin_vert
#version 450

layout(binding = 0) uniform pass_through_vs_params {
    mat4 mvp_mat;
};

layout(location = 0) in vec3 position;

void main() {
    gl_Position = mvp_mat * vec4(position, 1.0);
}
//@end

//@begin_frag
#version 450

layout(location = 0) out vec4 g_frag_color;

void main() {
    g_frag_color = vec4(1.0, 1.0, 1.0, 1.0);
}
//@end

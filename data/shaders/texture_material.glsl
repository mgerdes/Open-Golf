//@begin_vert
#version 450

layout(binding = 0) uniform texture_material_vs_params {
    mat4 model_mat;
    mat4 proj_view_mat;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texturecoord;
layout(location = 2) in vec3 normal;

layout(location = 0) out vec3 frag_position;
layout(location = 1) out vec2 frag_texturecoord;
layout(location = 2) out vec3 frag_normal;

void main() {
    frag_position = (model_mat * vec4(position, 1.0)).xyz;
    frag_texturecoord = texturecoord;
    frag_normal = normal;
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
//@end

//@begin_frag
#version 450

layout(binding = 0) uniform sampler2D texture_material_tex;

layout(location = 0) in vec3 frag_position;
layout(location = 1) in vec2 frag_texturecoord;
layout(location = 2) in vec3 frag_normal;

layout(location = 0) out vec4 g_frag_color;

void main() {
    vec3 color = texture(texture_material_tex, frag_texturecoord).xyz; 
    color = color + 0.001 * (frag_normal.xyz + frag_position.xyz);
    g_frag_color = vec4(color, 1.0);
}
//@end

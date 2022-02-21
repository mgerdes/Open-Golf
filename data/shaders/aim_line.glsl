//@begin_vert
#version 450
layout(binding = 0) uniform aim_line_vs_params {
    mat4 mvp_mat;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texture_coord;

layout(location = 0) out vec2 frag_texture_coord;

void main() {
    frag_texture_coord = texture_coord;
    gl_Position = mvp_mat * vec4(position, 1.0);
}
//@end

//@begin_frag
#version 450
layout(binding = 0) uniform aim_line_fs_params {
    vec4 color;
    vec2 texture_coord_offset, texture_coord_scale;
    float length0, length1;
    float total_length;
};
layout(binding = 0) uniform sampler2D aim_line_image;

layout(location = 0) in vec2 frag_texture_coord;

layout(location = 0) out vec4 g_frag_color;

void main() {
    float a = texture(aim_line_image, texture_coord_scale*frag_texture_coord + texture_coord_offset).a;
    float t = (length0 + (length1 - length0)*frag_texture_coord.x)/total_length; 
    float blend = 1.0f;
    if (t > 0.4f) {
        blend = (1.0f - (t - 0.4f) / 0.6f);
        a = a * blend * blend;
    }
    g_frag_color = vec4(color.xyz, color.w*a);
}
//@end

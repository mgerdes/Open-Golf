//@begin_vert
#version 450

layout(binding = 0) uniform ui_vs_params {
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

layout(binding = 0) uniform ui_fs_params {
    vec4 color;
    float alpha;
    float tex_x, tex_y, tex_dx, tex_dy, is_font;
};
layout(binding = 0) uniform sampler2D ui_texture;

layout(location = 0) in vec2 frag_texture_coord;

layout(location = 0) out vec4 g_frag_color;

void main() {
    vec2 tc = vec2(tex_x, tex_y) + frag_texture_coord * vec2(tex_dx, tex_dy);
    g_frag_color = texture(ui_texture, tc);
    g_frag_color.a = alpha * (is_font*g_frag_color.x + (1.0 - is_font)*g_frag_color.a);
    g_frag_color.xyz = (1.0 - is_font)*((1.0 - color.a)*g_frag_color.xyz + color.a*color.xyz) + is_font*color.xyz;
}
//@end

//@begin_vert
#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texture_coord;

layout(location = 0) out vec2 frag_texture_coord;

void main() {
    frag_texture_coord = texture_coord;
    gl_Position = vec4(position, -1);
}
//@end

//@begin_frag
#version 450

layout(binding = 0) uniform sampler2D render_image_texture;

layout(location = 0) in vec2 frag_texture_coord;

layout(location = 0) out vec4 g_frag_color;

void main() {
    g_frag_color = texture(render_image_texture, frag_texture_coord);
}
//@end

#version 330

uniform vec4 ui_single_color_fs_params[1];
layout(location = 0) out vec4 g_frag_color;
in vec2 frag_texture_coord;

void main()
{
    g_frag_color = ui_single_color_fs_params[0];
}


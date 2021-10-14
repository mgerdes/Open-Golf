#version 330

uniform vec4 ui_single_color_vs_params[4];
layout(location = 0) in vec3 position;
out vec2 frag_texture_coord;

void main()
{
    gl_Position = mat4(ui_single_color_vs_params[0], ui_single_color_vs_params[1], ui_single_color_vs_params[2], ui_single_color_vs_params[3]) * vec4(position, 1.0);
}

